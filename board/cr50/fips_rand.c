/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "cryptoc/util.h"
#include "dcrypto.h"
#include "fips.h"
#include "fips_rand.h"
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

static struct drbg_ctx fips_drbg;
static uint8_t _drbg_initialized;

#define ENTROPY_SIZE_WORDS (SHA256_DIGEST_WORDS * 2)
#define ENTROPY_SIZE (SHA256_DIGEST_SIZE * 2)

/**
 * buffer for entropy condensing. initialized during
 * fips_trng_startup(), but also used in KAT tests,
 * thus size is enough to accommodate needs
 */
static uint32_t entropy_fifo[ENTROPY_SIZE_WORDS];
static uint8_t noise_count;



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
		fips_throw_err(FIPS_FATAL_TRNG_RCT);
	if (cto + last_clo >= RCT_CUTOFF_SAMPLES)
		fips_throw_err(FIPS_FATAL_TRNG_RCT);

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
				fips_throw_err(FIPS_FATAL_TRNG_RCT);
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
		fips_throw_err(FIPS_FATAL_TRNG_APT);
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
	static uint16_t oldest;
	static uint16_t count; /* samples in statistics */
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
			fips_throw_err(FIPS_FATAL_TRNG_APT);
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
 * get random from TRNG and run continuous health tests.
 * it is also can simulate stuck-bit error
 * @param power_up if non-zero indicates warm-up mode
 * @return random value from TRNG
 */
static uint32_t fips_trng32(int power_up)
{
	uint32_t r;

	/* Continuous health tests should have been initialized by now */
	if (board_fips_enforced() && !power_up &&
	    (__rct_count < RCT_CUTOFF_WORDS || !__apt_initialized))
		fips_throw_err(FIPS_FATAL_TRNG_OTHER);

	/* get noise */
	r = rand();

	/* Simulate catastrophic hardware failure */
	if (fips_break_cmd == FIPS_BREAK_TRNG)
		r = 0;

	/* test #1: Repetition Count Test (a.k.a Stuck-bit) */
	repetition_count_test(r);
	/* warm-up test #2: Adaptive Proportion Test */
	adaptive_proportion_test(r);
	return r;
}

/**
 * FIPS-compliant TRNG startup.
 * The entropy source's startup tests shall run the continuous health tests
 * over at least 4096 consecutive samples.
 * Note: This function can throw FIPS_FATAL_TRNG error
 * And it have to be called 2 times, as it masks latency
 * Some number of samples will be available in entropy_fifo
 */
void fips_trng_startup(void)
{
	int i;

	__rct_count = 0;
	__apt_initialized = 0;
	/* Startup tests per NIST SP800-90B, Section 4 */
	/* 4096 1-bit samples, in 2 steps */
	for (i = 0; i < (TRNG_INIT_WORDS) / 2; i++) {
		uint32_t r = fips_trng32(1);
		/* store entropy for further use */
		entropy_fifo[i & (ENTROPY_SIZE_WORDS - 1)] = r;
	}
	noise_count = ENTROPY_SIZE_WORDS;
}


/* load fresh noise in entropy_fifo */
static void fill_noise(void)
{
	/* fill from the back */
	while (noise_count < ENTROPY_SIZE_WORDS)
		entropy_fifo[noise_count++] = fips_trng32(0);
}

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
	nonce = fips_trng32(0);

	/**
	 * not using get_entropy() as it needs a buffer for 440 bits
	 * which can't be allocated on stack due to stack limit, and
	 * allocating in bss wastes precious memory.
	 */
	fill_noise();
	hmac_drbg_init(&fips_drbg, &entropy_fifo, ENTROPY_SIZE, &nonce,
		       sizeof(nonce), NULL, 0);

	_drbg_initialized = 1;
	return EC_SUCCESS;
}

/* zeroize DRBG state */
void fips_drbg_clear(void)
{
	drbg_exit(&fips_drbg);
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

	return fips_hmac_drbg_generate_reseed(&fips_drbg, buffer, len,
					      NULL, 0);
}

/* return codes match dcrypto_p256_ecdsa_sign */
int fips_p256_ecdsa_sign(const p256_int *key, const p256_int *message,
			 p256_int *r, p256_int *s)
{
	if (!fips_crypto_allowed())
		return 0;
	if (!_drbg_initialized) {
		int err;

		err = fips_drbg_init();
		if (err)
			return 0;
	}
	return dcrypto_p256_ecdsa_sign(&fips_drbg, key, message, r, s);
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
			random_value = fips_trng32(0);
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
	ccprintf("rct_count = %d, noise_count = %d\n", __rct_count,
		 noise_count);
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
