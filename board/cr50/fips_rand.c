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
static struct drbg_ctx fips_drbg;

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
		throw_fips_err(FIPS_FATAL_TRNG);
	if (cto + last_clo >= RCT_CUTOFF_SAMPLES)
		throw_fips_err(FIPS_FATAL_TRNG);

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
				throw_fips_err(FIPS_FATAL_TRNG);
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
		throw_fips_err(FIPS_FATAL_TRNG);
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
			throw_fips_err(FIPS_FATAL_TRNG);
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

/*
 * ISR reacting to TRNG alert on TRNG_SECURE_POST_PROCESSING_CTRL_MONITOR_STATS
 */
static void trng_out_of_spec(void)
{
	fips_status |= FIPS_FATAL_TRNG;
}
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_TRNG0_OUT_OF_SPEC_ALERT_INT, trng_out_of_spec,
	    1);

/**
 * buffer for entropy condensing. initialized during
 * fips_trng_startup()
 */
static uint32_t entropy_fifo[SHA256_DIGEST_WORDS * 2];
/* fifo_left counts available full entropy */
static uint16_t full_entropy_left;
static uint16_t noise_to_fill;

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
		entropy_fifo[i & ((SHA256_DIGEST_WORDS * 2) - 1)] ^= r;
		/* warm-up test #1: Repetition Count Test (aka Stuck-bit) */
		repetition_count_test(r);
		/* warm-up test #2: Adaptive Proportion Test */
		adaptive_proportion_test(r);
	}
	fips_status |= FIPS_TRNG_TEST_COMPLETED;
	noise_to_fill = 0;
	full_entropy_left = 0;
}

uint32_t fips_trng32(void)
{
	uint32_t r;

	/* Continuous health tests should have been initialized by now */
	if (__rct_count < RCT_CUTOFF_WORDS || !__apt_initialized)
		throw_fips_err(FIPS_FATAL_TRNG);

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
	noise_to_fill = SHA256_DIGEST_WORDS * 2;
	/* since we filled buffer with noise, no full entropy yet */
	full_entropy_left = 0;
}

static void condense_entropy(void)
{
	/* make sure KAT passed, so we can use SHA2 */
	if (!(fips_status & FIPS_KAT_TEST_PASSED))
		init_fips();

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

/**
 * vetted post-processing using Hash for double block size
 * to get full entropy.
 * TODO(sukhomlinov): it's not yet used directly, may be remove it
 */
uint32_t get_entropy32(void)
{
	/* fifo_left and noise_left are in .bss and initialized to 0 */

	uint32_t out;

	/* make sure TRNG Health tests completed */
	if (!(fips_status & FIPS_TRNG_TEST_COMPLETED))
		fips_trng_startup();

	if (!full_entropy_left)
		condense_entropy();

	out = entropy_fifo[--full_entropy_left];
	/**
	 * continuously loading 2X of noise to avoid long stalls.
	 * fill from the end to not overwrite condensed entropy
	 * sharing same buffer.
	 */
	entropy_fifo[--noise_to_fill] = fips_trng32();
	entropy_fifo[--noise_to_fill] = fips_trng32();
	return out;
}

void get_entropy(void *buffer, size_t len)
{
	int random_togo = 0;
	uint32_t random_value;
	uint8_t *buf = (uint8_t *)buffer;

	/*
	 * Retrieve random numbers in 4 byte quantities and pack as many bytes
	 * as needed into 'buffer'. If len is not divisible by 4, the
	 * remaining random bytes get dropped.
	 */
	while (len--) {
		if (!random_togo) {
			random_value = get_entropy32();
			random_togo = sizeof(random_value);
		}
		*buf++ = (uint8_t)random_value;
		random_togo--;
		random_value >>= 8;
	}
}

void fips_drbg_init(void)
{
	uint32_t nonce;

	/* make sure TRNG is tested and KAT passed */
	if (!(fips_status & FIPS_KAT_TEST_PASSED))
		init_fips();

	if (!(fips_status & FIPS_TRNG_TEST_COMPLETED))
		fips_trng_startup();

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
	hmac_drbg_init(&fips_drbg, &entropy_fifo, sizeof(entropy_fifo), &nonce,
		       sizeof(nonce), NULL, 0);

	fips_status |= FIPS_DRBG_INITIALIZED;
}

/* zeroize DRBG state */
void fips_drbg_clear(void)
{
	drbg_exit(&fips_drbg);
	fips_status &= ~FIPS_DRBG_INITIALIZED;
}

void fips_rand_bytes(void *buffer, size_t len)
{
	int err;
	/**
	 * make sure cr50 DRBG is initialized after power-on or resume,
	 * but do it on first use to minimize latency of board_init()
	 */
	if (!(fips_status & FIPS_DRBG_INITIALIZED))
		fips_drbg_init();

	err = hmac_drbg_generate(&fips_drbg, buffer, len, NULL, 0);
	/**
	 *  if reseed is required, do it. handle future health tests
	 */
	while (err) {
		condense_entropy();
		hmac_drbg_reseed(&fips_drbg, entropy_fifo, SHA256_DIGEST_SIZE,
				 NULL, 0, NULL, 0);
		/* mark that we used all entropy */
		full_entropy_left = 0;
		err = hmac_drbg_generate(&fips_drbg, buffer, len, NULL, 0);
	}
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
 * type      |    1     | 0 = TRNG, 1 = CR50 DRBG, 2 = get_entropy
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
		get_entropy(buf, text_len);
		break;
	case 3:
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

	fips_status = FIPS_UNINITIALIZED;
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
	starttime = get_time().val;
	for (k = 0; k < 20; k++) {
		for (j = 0; j < 50; j++)
			get_entropy(&buf, sizeof(buf));
		watchdog_reload();
	}
	starttime = get_time().val - starttime;
	ccprintf("time for 1000 entropy reads = %llu\n", starttime);
	cflush();

	return 0;
}

DECLARE_SAFE_CONSOLE_COMMAND(rand_perf, cmd_rand_perf, NULL, NULL);

#endif /* CRYPTO_TEST_SETUP */
