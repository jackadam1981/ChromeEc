/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "cryptoc/util.h"
#include "dcrypto.h"
#include "fips.h"
#include "fips_rand.h"
#include "flash_log.h"
#include "init_chip.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "trng.h"

/**
 * FIPS-compliant CR50-wide DRBG
 * Since raw TRNG input shouldn't be used as random number generator,
 * all FIPS-compliant code use DRBG, seeded from TRNG
 */

/* reusable buffer for KAT tests to avoid stack overflow */
static union {
	LITE_HMAC_CTX hmac_ctx;
	struct drbg_ctx fips_drbg;
} kat_buf;
static uint8_t _drbg_initialized;

#define ENTROPY_SIZE_WORDS (SHA256_DIGEST_WORDS * 2)
#define ENTROPY_SIZE (SHA256_DIGEST_SIZE * 2)

/**
 * buffer for entropy condensing. initialized during
 * fips_trng_startup(), but also used in KAT tests,
 * thus size is enough to accommodate needs
 */
static uint32_t entropy_fifo[SHA256_DIGEST_WORDS * 4];
/* fifo_left counts available full entropy */
static uint16_t full_entropy_left;
static uint16_t noise_to_fill;

/* Test values from OpenSSL */
void fips_sha256_kat(void)
{
	static const uint8_t in[] = /* "etaonrishd" */ {0x65, 0x74, 0x61, 0x6f,
		0x6e, 0x72, 0x69, 0x73, 0x68, 0x64};
	static const uint8_t ans[] = {0xf5, 0x53, 0xcd, 0xb8, 0xcf, 0x1, 0xee,
		0x17, 0x9b, 0x93, 0xc9, 0x68, 0xc0, 0xea, 0x40, 0x91, 0x6,
		0xec, 0x8e, 0x11, 0x96, 0xc8, 0x5d, 0x1c, 0xaf, 0x64, 0x22,
		0xe6, 0x50, 0x4f, 0x47, 0x57};

	DCRYPTO_SHA256_init(&kat_buf.hmac_ctx.hash, 0);
	HASH_update(&kat_buf.hmac_ctx.hash, in, sizeof(in));
	if (memcmp(HASH_final(&kat_buf.hmac_ctx.hash), ans, SHA256_DIGEST_SIZE))
		fips_throw_err(FIPS_FATAL_SHA256);
}

/* Test values from OpenSSL */
void fips_hmac_sha256_kat(void)
{
	static const uint8_t k[SHA256_DIGEST_SIZE] = /* "etaonrishd" */ {0x65,
		0x74, 0x61, 0x6f, 0x6e, 0x72, 0x69, 0x73, 0x68, 0x64, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00};
	static const uint8_t in[] = /* "Sample text" */ {0x53, 0x61, 0x6d, 0x70,
		0x6c, 0x65, 0x20, 0x74, 0x65, 0x78, 0x74};
	static const uint8_t ans[] = { 0xe9, 0x17, 0xc1, 0x7b, 0x4c, 0x6b, 0x77,
				       0xda, 0xd2, 0x30, 0x36, 0x02, 0xf5, 0x72,
				       0x33, 0x87, 0x9f, 0xc6, 0x6e, 0x7b, 0x7e,
				       0xa8, 0xea, 0xaa, 0x9f, 0xba, 0xee, 0x51,
				       0xff, 0xda, 0x24, 0xf4 };

	DCRYPTO_HMAC_SHA256_init(&kat_buf.hmac_ctx, k, sizeof(k));
	HASH_update(&kat_buf.hmac_ctx.hash, in, sizeof(in));
	if (memcmp(DCRYPTO_HMAC_final(&kat_buf.hmac_ctx), ans,
		   SHA256_DIGEST_SIZE))
		fips_throw_err(FIPS_FATAL_HMAC_SHA256);
}

static const uint8_t drbg_entropy0[] = {
	0x42, 0x94, 0x67, 0x1d, 0x49, 0x3d, 0xc0, 0x85, 0xb5, 0x18, 0x46,
	0x07, 0xd7, 0xde, 0x2f, 0xf2, 0xb6, 0xac, 0xeb, 0x73, 0x4a, 0x1b,
	0x02, 0x6f, 0x6c, 0xfe, 0xe7, 0xc5, 0xa9, 0x0f, 0x03, 0xda
};
static const uint8_t drbg_nonce0[] = { 0xd0, 0x71, 0x54, 0x4e,
						0x59, 0x92, 0x35, 0xd5,
						0xeb, 0x38, 0xb6, 0x4b,
						0x55, 0x1d, 0x2a, 0x6e };
static const uint8_t drbg_perso0[] = {
	0x63, 0xbc, 0x76, 0x9a, 0xe1, 0xd9, 0x5a, 0x98, 0xbd, 0xe8, 0x70,
	0xe4, 0xdb, 0x77, 0x76, 0x29, 0x70, 0x41, 0xd3, 0x7c, 0x8a, 0x5c,
	0x68, 0x8d, 0x4e, 0x02, 0x4b, 0x78, 0xd8, 0x3f, 0x4d, 0x78
};

static const uint8_t drbg_entropy1[] = {
	0xdb, 0x9b, 0x47, 0x90, 0xb6, 0x23, 0x36, 0xfb, 0xb9, 0xa6, 0x84,
	0xb8, 0x29, 0x47, 0x06, 0x53, 0x93, 0xee, 0xef, 0x8f, 0x57, 0xbd,
	0x24, 0x77, 0x14, 0x1a, 0xd1, 0x7e, 0x77, 0x6d, 0xac, 0x34
};
static const uint8_t drbg_addtl_input1[] = {
	0x28, 0x84, 0x8b, 0xec, 0xd3, 0xf4, 0x76, 0x96, 0xf1, 0x24, 0xf4,
	0xb1, 0x48, 0x53, 0xa4, 0x56, 0x15, 0x6f, 0x69, 0xbe, 0x58, 0x3a,
	0x7d, 0x46, 0x82, 0xcf, 0xf8, 0xd4, 0x4b, 0x39, 0xe1, 0xd3
};

static const uint8_t drbg_entropy2[] = {
	0x4a, 0x9a, 0xbe, 0x80, 0xf6, 0xf5, 0x22, 0xf2, 0x98, 0x78, 0xbe,
	0xdf, 0x82, 0x45, 0xb2, 0x79, 0x40, 0xa7, 0x64, 0x71, 0x00, 0x6f,
	0xb4, 0xa4, 0x11, 0x0b, 0xeb, 0x4d, 0xec, 0xb6, 0xc3, 0x41
};
static const uint8_t drbg_addtl_input2[] = {
	0x8b, 0xfc, 0xe0, 0xb7, 0x13, 0x26, 0x61, 0xc3, 0xcd, 0x78, 0x17,
	0x5d, 0x83, 0x92, 0x6f, 0x64, 0x3e, 0x36, 0xf7, 0x60, 0x8e, 0xec,
	0x2c, 0x5d, 0xac, 0x3d, 0xdc, 0xba, 0xcc, 0x8c, 0x21, 0x82
};

/**
 *  DRBG test vector source recorded 6/1/17 from
 * http://csrc.nist.gov/groups/STM/cavp/documents/drbg/drbgtestvectors.zip,
 * Input values:
 * [SHA-256]
 * [PredictionResistance = True]
 * [EntropyInputLen = 256]
 * [NonceLen = 128]
 * [PersonalizationStringLen = 256]
 * [AdditionalInputLen = 256]
 * [ReturnedBitsLen = 1024]
 * COUNT = 0
 * EntropyInput =
 * 4294671d493dc085b5184607d7de2ff2b6aceb734a1b026f6cfee7c5a90f03da
 * Nonce = d071544e599235d5eb38b64b551d2a6e
 * PersonalizationString =
 * 63bc769ae1d95a98bde870e4db7776297041d37c8a5c688d4e024b78d83f4d78
 * AdditionalInput =
 * 28848becd3f47696f124f4b14853a456156f69be583a7d4682cff8d44b39e1d3
 * EntropyInputPR =
 * db9b4790b62336fbb9a684b82947065393eeef8f57bd2477141ad17e776dac34
 * AdditionalInput =
 * 8bfce0b7132661c3cd78175d83926f643e36f7608eec2c5dac3ddcbacc8c2182
 * EntropyInputPR =
 * 4a9abe80f6f522f29878bedf8245b27940a76471006fb4a4110beb4decb6c341
 * ReturnedBits =
 * e580dc969194b2b18a97478aef9d1a72390aff14562747bf080d741527a6655
 * ce7fc135325b457483a9f9c70f91165a811cf4524b50d51199a0df3bd60d12abac27d0bf6618
 * e6b114e05420352e23f3603dfe8a225dc19b3d1fff1dc245dc6b1df24c741744bec3f9437dbb
 * f222df84881a457a589e7815ef132f686b760f012

 * DRBG KAT generation sequence:
 * DRBG_init(entropy0, nonce0, perso0)
 * DRBG_reseed(entropy1, addtl_input1)
 * DRBG_generate()
 * DRBG_reseed(entropy2, addtl_input2)
 * DRBG_generate()
 */
static void fips_hmac_drbg_instantiate_kat(struct drbg_ctx *ctx)
{
	/* Expected internal drbg state */
	static const uint32_t K0[] = { 0x7fe2b43a, 0x94f11b33, 0x2b76c5ce,
				       0xfbb784af, 0x81cfe716, 0xc43596d6,
				       0xbdfe968b, 0x189c80fb };
	static const uint32_t V0[] = { 0xc42b237a, 0x929cdd0b, 0xe7fbafdd,
				       0xba22a36a, 0x4d23471a, 0xd8607022,
				       0x687e18ac, 0x2ac08007 };

	hmac_drbg_init(ctx, drbg_entropy0, sizeof(drbg_entropy0), drbg_nonce0,
		  sizeof(drbg_nonce0), drbg_perso0, sizeof(drbg_perso0));

	if (memcmp(ctx->v, V0, sizeof(V0)) || memcmp(ctx->k, K0, sizeof(K0)))
		fips_throw_err(FIPS_FATAL_HMAC_DRBG);
}

static void fips_hmac_drbg_reseed_kat(struct drbg_ctx *ctx)
{
	/* Expected internal drbg state */
	static const uint32_t K1[] = { 0x3118D36E, 0x05DEEC48, 0x7EFB6363,
				       0x3D575CDE, 0xCFCD14C1, 0x8D4F937D,
				       0x896B811E, 0x0EF038EB };
	static const uint32_t V1[] = { 0xC8ED8EEC, 0x24DD7B66, 0x09C635CD,
				       0x6AC74196, 0xC70067D7, 0xC2E71FEF,
				       0x918D9EB7, 0xAE0CD544 };

	hmac_drbg_reseed(ctx, drbg_entropy1, sizeof(drbg_entropy1),
		    drbg_addtl_input1, sizeof(drbg_addtl_input1), NULL, 0);

	if (memcmp(ctx->v, V1, sizeof(V1)) || memcmp(ctx->k, K1, sizeof(K1)))
		fips_throw_err(FIPS_FATAL_HMAC_DRBG);
}

static void fips_hmac_drbg_generate_kat(struct drbg_ctx *ctx)
{
	/* Expected internal drbg state */
	static const uint32_t K2[] = { 0x980ccd6a, 0x0b34f7e1, 0x594aabd7,
				       0x33b66049, 0xb919bd57, 0x8ecc7194,
				       0xaf1748a3, 0x80982577 };
	static const uint32_t V2[] = { 0xe4927cdb, 0xb3435cc5, 0x601ab870,
				       0x46e1f024, 0x966ca875, 0x102b4167,
				       0xa71e5dce, 0xe4c15962 };
	/* Expected output */
	static const uint8_t KA[] = {
		0xe5, 0x80, 0xdc, 0x96, 0x91, 0x94, 0xb2, 0xb1, 0x8a, 0x97,
		0x47, 0x8a, 0xef, 0x9d, 0x1a, 0x72, 0x39, 0x0a, 0xff, 0x14,
		0x56, 0x27, 0x47, 0xbf, 0x08, 0x0d, 0x74, 0x15, 0x27, 0xa6,
		0x65, 0x5c, 0xe7, 0xfc, 0x13, 0x53, 0x25, 0xb4, 0x57, 0x48,
		0x3a, 0x9f, 0x9c, 0x70, 0xf9, 0x11, 0x65, 0xa8, 0x11, 0xcf,
		0x45, 0x24, 0xb5, 0x0d, 0x51, 0x19, 0x9a, 0x0d, 0xf3, 0xbd,
		0x60, 0xd1, 0x2a, 0xba, 0xc2, 0x7d, 0x0b, 0xf6, 0x61, 0x8e,
		0x6b, 0x11, 0x4e, 0x05, 0x42, 0x03, 0x52, 0xe2, 0x3f, 0x36,
		0x03, 0xdf, 0xe8, 0xa2, 0x25, 0xdc, 0x19, 0xb3, 0xd1, 0xff,
		0xf1, 0xdc, 0x24, 0x5d, 0xc6, 0xb1, 0xdf, 0x24, 0xc7, 0x41,
		0x74, 0x4b, 0xec, 0x3f, 0x94, 0x37, 0xdb, 0xbf, 0x22, 0x2d,
		0xf8, 0x48, 0x81, 0xa4, 0x57, 0xa5, 0x89, 0xe7, 0x81, 0x5e,
		0xf1, 0x32, 0xf6, 0x86, 0xb7, 0x60, 0xf0, 0x12
	};

	hmac_drbg_generate(ctx, entropy_fifo, sizeof(entropy_fifo), NULL, 0);
	/* Verify internal drbg state */
	if (memcmp(ctx->v, V2, sizeof(V2)) || memcmp(ctx->k, K2, sizeof(K2))) {
		fips_throw_err(FIPS_FATAL_HMAC_DRBG);
		return;
	}

	hmac_drbg_reseed(ctx, drbg_entropy2, sizeof(drbg_entropy2),
		    drbg_addtl_input2, sizeof(drbg_addtl_input2), NULL, 0);
	/**
	 * reuse entropy buffer to avoid allocating too much stack and memory
	 * it will be cleaned up in TRNG health test
	 */
	hmac_drbg_generate(ctx, entropy_fifo, sizeof(entropy_fifo), NULL, 0);
	if (memcmp(entropy_fifo, KA, sizeof(KA)))
		fips_throw_err(FIPS_FATAL_HMAC_DRBG);
}

void fips_hmac_drbg_kat(void)
{
	fips_hmac_drbg_instantiate_kat(&kat_buf.fips_drbg);
	fips_hmac_drbg_reseed_kat(&kat_buf.fips_drbg);
	fips_hmac_drbg_generate_kat(&kat_buf.fips_drbg);
}


/**
 * NIST FIPS TRNG health tests (NIST SP 800-90B 4.3)
 * If any of the approved continuous health tests are used by the entropy
 * source, the false positive probability for these tests shall be set to
 * at least 2^-50
 */

/**
 * NIST SP 800-90B 4.4.1
 * The repition count test detects abnormal runs of 0s or 1s.
 * RCT_CUTOFF_BITS must be >= 32.
 * If a single value appears more than 100/H times in a row,
 * the tests must detect this with high probability.
 */
static uint8_t __rct_count;
void repetition_count_test(uint32_t rnd)
{
#if TRNG_SAMPLE_BITS == 1
	static int last_clz;
	static int last_clo;
	uint32_t clz, ctz, clo, cto;

	clz = __builtin_clz(rnd);
	ctz = __builtin_ctz(rnd);
	clo = __builtin_clz(~rnd);
	cto = __builtin_ctz(~rnd);

	if (ctz + last_clz >= RCT_CUTOFF_SAMPLES)
		fips_throw_err(FIPS_FATAL_TRNG);
	if (cto + last_clo >= RCT_CUTOFF_SAMPLES)
		fips_throw_err(FIPS_FATAL_TRNG);

	last_clz = clz + ((!!rnd - 1) & last_clz);
	last_clo = clo + ((!!~rnd - 1) & last_clo);
#else
	static uint8_t prev, prev_count;
	static uint8_t bits, val;
	/* add remaining bits from previous TRNG reading */
	val = (val | (rnd << bits)) & ((1 << TRNG_SAMPLE_BITS) - 1);
	rnd >>= TRNG_SAMPLE_BITS - bits;
	bits += 32;
	while (bits >= TRNG_SAMPLE_BITS) {
		if (val == prev) {
			prev_count++;
			if (prev_count >= RCT_CUTOFF_SAMPLES)
				fips_throw_err(FIPS_FATAL_TRNG);
		} else {
			prev = val;
			prev_count = 1;
		}
		val = rnd & ((1 << TRNG_SAMPLE_BITS) - 1);
		rnd >>= TRNG_SAMPLE_BITS;
		bits -= TRNG_SAMPLE_BITS;
	};
#endif
	if (__rct_count < RCT_CUTOFF_WORDS)
		++__rct_count;
}

static uint8_t __apt_initialized;

#if TRNG_SAMPLE_BITS == 1
static int misbalanced(uint32_t count)
{
	return count > APT_CUTOFF_SAMPLES ||
	       count < APT_WINDOW_SIZE_BITS - APT_CUTOFF_SAMPLES;
}

/* Population count */
static int popcount(uint32_t x)
{
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F;
	x = x + (x >> 8);
	x = x + (x >> 16);
	return x & 0x0000003F;
}

/**
 * NIST SP 800-90B 4.4.2 Adaptive Proportion Test.
 * Implementation for 1-bit alphabet.
 * Instead of storing actual samples we can store pop counts
 * of each 32bit reading, which would fit in 8-bit.
 */
void adaptive_proportion_test(uint32_t rnd)
{
	static uint8_t pops[APT_WINDOW_SIZE_NWORDS];
	static uint32_t oldest;
	static uint32_t count;

	/* update rolling count */
	count -= pops[oldest];
	pops[oldest] = popcount(rnd);
	count += pops[oldest];
	if (++oldest >= APT_WINDOW_SIZE_NWORDS) {
		__apt_initialized = 1; /* saw full window */
		oldest = 0;
	}

	/* check when initialized */
	if (__apt_initialized && misbalanced(count))
		fips_throw_err(FIPS_FATAL_TRNG);
}
#else
/**
 * NIST SP 800-90B 4.4.2 Adaptive Proportion Test.
 * Variant for multi-bit TRNG alphabet
 * Naive implementation use 512 bytes to store samples. Performance is low.
 * Can be further optimized to:
 *      - process samples in packed form (save memory)
 *      - maintain frequency table (improve speed)
 */
void adaptive_proportion_test(uint32_t rnd)
{
	static uint32_t oldest;
	static uint32_t count; /* samples in statistics */
	static uint8_t samples[APT_WINDOW_SIZE_SAMPLES];
	static uint8_t bits, val;
	size_t i;

	/* add remaining bits from previous TRNG reading */
	val = (val | (rnd << bits)) & ((1U << TRNG_SAMPLE_BITS) - 1);
	rnd >>= TRNG_SAMPLE_BITS - bits;
	bits = 32 + bits;
	while (bits >= TRNG_SAMPLE_BITS) {
		count = 0;
		/* TODO: we can store samples in packed form */
		if (__apt_initialized)
			for (i = 0; i < APT_WINDOW_SIZE_SAMPLES; i++)
				count += samples[i] == val;

		samples[oldest] = val;
		if (count > APT_CUTOFF_SAMPLES)
			fips_throw_err(FIPS_FATAL_TRNG);
		val = rnd & ((1 << TRNG_SAMPLE_BITS) - 1);
		rnd >>= TRNG_SAMPLE_BITS;
		bits -= TRNG_SAMPLE_BITS;
		if (++oldest >= APT_WINDOW_SIZE_SAMPLES) {
			__apt_initialized = 1; /* saw full window */
			oldest = 0;
		}
	};
}
#endif


/**
 * FIPS-compliant TRNG startup.
 * The entropy source's startup tests shall run the continuous health tests
 * over at least 4096 consecutive samples.
 * Note: This function can set the global 'fips_error' variable.
 */
void fips_trng_startup(void)
{
	int i;

	__rct_count = 0;
	__apt_initialized = 0;
	/* Startup tests per NIST SP800-90B, Section 4 */
	/* 4096 1-bit samples */
	for (i = 0; i < (TRNG_INIT_WORDS); i++) {
		uint32_t r = rand();
		/* store entropy for further use */
		entropy_fifo[i & (ENTROPY_SIZE_WORDS - 1)] ^= r;
		/* warm-up test #1: Repetition Count Test (aka Stuck-bit) */
		repetition_count_test(r);
		/* warm-up test #2: Adaptive Proportion Test */
		adaptive_proportion_test(r);
	}
	noise_to_fill = 0;
	full_entropy_left = 0;
}

uint32_t fips_trng32(void)
{
	uint32_t r;

	/* Continuous health tests should have been initialized by now */
	if (__rct_count < RCT_CUTOFF_WORDS || !__apt_initialized)
		fips_throw_err(FIPS_FATAL_TRNG);

	/* get noise */
	r = rand();

#ifdef FIPS_TEST_ERR_HANDLING
	/* Simulate catastrophic hardware failure */
	if (break_cmd == TRNG_RCT)
		r = 0;
#endif

	/* Add sample to continuous health tests */
	repetition_count_test(r);
	adaptive_proportion_test(r);
	return r;
}

/* load fresh noise in entropy_fifo */
static void fill_noise(void)
{
	while (noise_to_fill)
		entropy_fifo[--noise_to_fill] = fips_trng32();
	/**
	 * noise is expected to be consumed as is or be condensed,
	 * so mark it for full reload
	 */
	noise_to_fill = ENTROPY_SIZE_WORDS;
	/* since we filled buffer with noise, no full entropy yet */
	full_entropy_left = 0;
}
#if 0
static int condense_entropy(void)
{
	/* make sure KAT passed, so we can use SHA2 */
	if (!fips_crypto_allowed())
		return EC_ERROR_INVALID_CONFIG;

	/**
	 * entropy fifo is first initialized with noise during initial
	 * health tests in fips_trng_startup(). here we make sure
	 * we have 2 SHA2-256 blocks of fresh entropy.
	 */
	fill_noise();

	/**
	 * Vetted Conditioning Components - HASH SHA2-256
	 * When the input entropy is at least 2×min(nout, q),
	 * nout full-entropy output bits are produced.
	 */
	DCRYPTO_SHA256_hash(entropy_fifo, sizeof(entropy_fifo),
			    (uint8_t *)entropy_fifo);
	full_entropy_left = SHA256_DIGEST_WORDS;
}
#endif

int fips_drbg_init(void)
{
	uint32_t nonce;

	if (!fips_crypto_allowed())
		return EC_ERROR_INVALID_CONFIG;

	/**
	 * initialize DRBG with 440 bits of entropy as required
	 * by NIST SP 800-90A 10.1. Includes entropy and nonce,
	 * both received from entropy source.
	 * entropy_fifo contains 512 bits of noise with H>= 0.85
	 * this is roughly equal to 435 bits of full entropy.
	 * Add 32 * 0.85 = 27 bits from nonce.
	 */
	nonce = fips_trng32();

	/**
	 * not using get_entropy() as it needs a buffer for 440 bits
	 * which can't be allocated on stack due to stack limit, and
	 * allocating in bss wastes precious memory.
	 */
	fill_noise();
	hmac_drbg_init(&kat_buf.fips_drbg, &entropy_fifo, ENTROPY_SIZE, &nonce,
		       sizeof(nonce), NULL, 0);

	_drbg_initialized = 1;
	return EC_SUCCESS;
}

/* zeroize DRBG state */
void fips_drbg_clear(void)
{
	drbg_exit(&kat_buf.fips_drbg);
	_drbg_initialized = 0;
}

enum hmac_result fips_hmac_drbg_generate_reseed(struct drbg_ctx *ctx, void *out,
						size_t out_len,
						const void *input,
						size_t input_len)
{
	int err;

	err = hmac_drbg_generate(ctx, out, out_len, input, input_len);

	while (err == HMAC_DRBG_RESEED_REQUIRED) {
		fill_noise();
		hmac_drbg_reseed(ctx, entropy_fifo, ENTROPY_SIZE, NULL,
				 0, NULL, 0);
		/* mark that we used all entropy */
		full_entropy_left = 0;
		err = hmac_drbg_generate(ctx, out, out_len, input, input_len);
	}
	return (err) ? EC_ERROR_INVAL : EC_SUCCESS;
}

int fips_rand_bytes(void *buffer, size_t len)
{
	int err;

	if (!fips_crypto_allowed())
		return EC_ERROR_INVALID_CONFIG;
	/**
	 * make sure cr50 DRBG is initialized after power-on or resume,
	 * but do it on first use to minimize latency of board_init()
	 */
	if (!_drbg_initialized) {
		err = fips_drbg_init();
		if (!err)
			return err;
	}

	return fips_hmac_drbg_generate_reseed(&kat_buf.fips_drbg, buffer, len,
					      NULL, 0);
}

int fips_p256_ecdsa_sign(const p256_int *key, const p256_int *message,
			 p256_int *r, p256_int *s)
{
	if (!fips_crypto_allowed())
		return EC_ERROR_INVALID_CONFIG;

	if (!_drbg_initialized) {
		int err;

		err = fips_drbg_init();
		if (!err)
			return err;
	}
	return dcrypto_p256_ecdsa_sign(&kat_buf.fips_drbg, key, message, r, s);
}

uint32_t fips_rand(void)
{
	uint32_t out;

	fips_rand_bytes(&out, sizeof(out));
	return out;
}

#if !defined(SECTION_IS_RO) && defined(CRYPTO_TEST_SETUP)
#include "extension.h"
#include "watchdog.h"

void fips_trng_bytes(void *buffer, size_t len)
{
	uint8_t *buf = (uint8_t *)buffer;
	uint32_t random_value, random_togo = 0;
	/**
	 * Retrieve random numbers in 4 byte quantities and pack as many bytes
	 * as needed into 'buffer'. If len is not divisible by 4, the
	 * remaining random bytes get dropped.
	 */
	while (len--) {
		if (!random_togo) {
			random_value = fips_trng32();
			random_togo = sizeof(random_value);
		}
		*buf++ = (uint8_t)random_value;
		random_togo--;
		random_value >>= 8;
	}
}

/*
 * This extension command is similar to TPM2_GetRandom, but made
 * available for CRYPTO_TEST = 1 which disables TPM
 * Command structure, shared out of band with the test driver running
 * on the host:
 *
 * field     |    size  |                  note
 * =========================================================================
 * text_len  |    2     | size of the text to process, big endian
 * type      |    1     | 0 = TRNG, 1 = CR50 DRBG, 2 = fips_trng_bytes
 */
static enum vendor_cmd_rc trng_test(enum vendor_cmd_cc code, void *buf,
				    size_t input_size, size_t *response_size)
{
	uint16_t text_len;
	uint8_t *cmd;
	uint8_t op_type = 0;
	size_t response_room = *response_size;

	*response_size = 0;
	if (input_size != sizeof(text_len) + 1)
		return VENDOR_RC_BOGUS_ARGS;

	cmd = buf;
	text_len = *cmd++;
	text_len = text_len * 256 + *cmd++;
	text_len = MIN(text_len, response_room);

	op_type = *cmd++;

	switch (op_type) {
	case 0:
		rand_bytes(buf, text_len);
		break;
	case 1:
		fips_rand_bytes(buf, text_len);
		break;
	case 2:
		fips_trng_bytes(buf, text_len);
		break;
	default:
		return VENDOR_RC_BOGUS_ARGS;
	}
	*response_size = text_len;
	return VENDOR_RC_SUCCESS;
}

DECLARE_VENDOR_COMMAND(VENDOR_CC_TRNG_TEST, trng_test);

static int cmd_rand_perf(int argc, char **argv)
{
	uint64_t starttime;
	static uint32_t buf[SHA256_DIGEST_WORDS];
	int j, k;

	starttime = get_time().val;
	fips_trng_startup();
	starttime = get_time().val - starttime;
	ccprintf("trng_alphabet=%d, time for fips_trng_startup = %llu\n",
		 TRNG_SAMPLE_BITS, starttime);
	ccprintf("rct_count = %d, noise_to_fill = %d\n", __rct_count,
		 noise_to_fill);
	cflush();

	starttime = get_time().val;
	fips_drbg_init();
	starttime = get_time().val - starttime;
	ccprintf("time for drbg_init = %llu\n", starttime);
	cflush();
	starttime = get_time().val;
	for (k = 0; k < 10; k++) {
		for (j = 0; j < 100; j++)
			fips_rand_bytes(buf, sizeof(buf));
		watchdog_reload();
		cflush();
	}
	starttime = get_time().val - starttime;
	ccprintf("time for 1000 drbg reads = %llu\n", starttime);
	cflush();

	starttime = get_time().val;
	for (k = 0; k < 10; k++) {
		for (j = 0; j < 100; j++)
			rand_bytes(&buf, sizeof(buf));
		watchdog_reload();
	}
	starttime = get_time().val - starttime;
	ccprintf("time for 1000 rand_byte() = %llu\n", starttime);
	cflush();

	return 0;
}

DECLARE_SAFE_CONSOLE_COMMAND(rand_perf, cmd_rand_perf, NULL, NULL);

#endif /* CRYPTO_TEST_SETUP */
