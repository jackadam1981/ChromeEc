/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "trng.h"

#include <stdbool.h>

#include "console.h"
#include "dcrypto.h"
#include "init_chip.h"
#include "panic.h"
#include "registers.h"

/*
 *  This code initializes an AES based CPRNG from the TRNG during init_trng().
 *  It then runs a simple health check to determine that the LSB of each TRNG
 *  sample is not well correlated with the prior 2 sample LSBs.  The original
 *  trng.c remains for use in the RO code.  The resulting CPRNG output should
 *  be less predictable and significantly faster than the output from the
 *  hardware TRNG.
 */

/* Number of TRNG samples required before starting the CPRNG. */
#define CPRNG_INIT_SAMPLES 128

/*
 * Allowed bias as a percent.  The number of 1's vs 0's, given the prior prefix
 * bits, needs to be within this factor of the expected number.
 */
#define REQUIRED_UNPREDICTABILITY 80
/* Number of prior sample LSBs used to predict the next TRNG sample LSB. */
#define NUM_TRNG_PREFIX_BITS 2
#define NONCE_LEN 16
#define KEY_LEN 16
#define RAND_BUF_LEN (NONCE_LEN + KEY_LEN)

/*
 * We have three views of the CPRNG state.  The TRNG wants to fill a buffer
 * with random data, which is the |rand_buf| view, which is a flat 8-word
 * array of uint32_t.  The CPRNG wants to use the state as a 16-byte AES nonce,
 * and a 16-byte AES key.  We only increment the low 8 bytes of the nonce, so
 * there is a uint64_t |counter| view.
 */
struct CPRNG_state {
	uint8_t nonce[NONCE_LEN];
	uint8_t key[KEY_LEN];
};

union CPRNG_union {
  uint32_t rand_buf[RAND_BUF_LEN / sizeof(uint32_t)];
  struct CPRNG_state state;
  uint64_t counter;  /* This will be the first 8 bytes of the nonce. */
};

static volatile union CPRNG_union global_CPRNG_union;
static volatile uint16_t global_buf_index;
static volatile bool global_is_healthy;

/*
 * Use a simple TRNG health check before starting the CPRNG.  We believe the low
 * bit is fairly random, with possibly some bias independent of other samples.
 * Check that the bias is not too high, and that we are independent of the
 * previous 3 LSBs.  If this is true to enough accuracy, start the CPRNG.
 */
uint16_t global_num_ones[1 << NUM_TRNG_PREFIX_BITS];
uint16_t global_num_zeros[1 << NUM_TRNG_PREFIX_BITS];
/* Keep track of the LSB of previous samples. */
volatile uint8_t global_prefix_lsbs = 0;
volatile uint32_t global_total_samples;

/* Return a raw random number from the TRNG. */
uint32_t hardware_raw_rand(void)
{
	while (GREAD(TRNG, EMPTY)) {
		if (GREAD_FIELD(TRNG, FSM_STATE, FSM_TIMEOUT)) {
			/* TRNG timed out, restart */
			GWRITE(TRNG, STOP_WORK, 1);
			GWRITE(TRNG, GO_EVENT, 1);
		}
	}
	return GREAD(TRNG, READ_DATA);
}

/* Reset TRNG stats. */
static void reset_trng_stats(void)
{
	global_is_healthy = false;
	global_buf_index = 0;
}

/*
 * Count how many times we see 1 or 0 in the sample LSB, given the prior 3 bits.
 */
static void collectLSBStates(uint32_t sample)
{
	uint8_t prefix;

	if (global_buf_index >= NUM_TRNG_PREFIX_BITS) {
		prefix = global_prefix_lsbs & ((1 << NUM_TRNG_PREFIX_BITS) - 1);
		if (sample & 1)
			++global_num_ones[prefix];
		else
			++global_num_zeros[prefix];
		++global_total_samples;
	}
	global_prefix_lsbs <<= 1;
	global_prefix_lsbs |= sample & 1;
}

/* Rotate left by |n|. */
static inline uint16_t rol(uint32_t val, uint32_t n)
{
	if (n != 0)
		val = (val << n) | (val >> (16 - n));
	return val;
}

/*
 * Read CPRNG_INIT_SAMPLES samples.  Mix them into global_CPRNG_state.rand_buf,
 * and track stats on their LSBs, which are the most random.
 */
static void read_samples(void)
{
	uint32_t sample;
	uint16_t i, index;

	for (i = 0; i < CPRNG_INIT_SAMPLES; i++) {
		sample = hardware_raw_rand();
		collectLSBStates(sample);
		index = global_buf_index &
			((RAND_BUF_LEN / sizeof(uint32_t)) - 1);
		global_CPRNG_union.rand_buf[index] =
			rol(global_CPRNG_union.rand_buf[index], 1) ^ sample;
		++global_buf_index;
	}
}

/*
 * Check that the TRNG data collected appears to have enough unpredictability.
 */
static bool is_healthy(void)
{
	uint8_t i;
	uint32_t expected_count;
	uint16_t min_count, max_count;
	bool passed = true;

	/* ccprintf("Checking TRNG health\n"); */
	expected_count = global_total_samples >> (NUM_TRNG_PREFIX_BITS + 1);
	min_count = (expected_count * REQUIRED_UNPREDICTABILITY) / 100;
	max_count = (expected_count * 100) / REQUIRED_UNPREDICTABILITY;
	/*ccprintf("min count = %u, max count = %u\n", min_count, max_count);*/
	for (i = 0; i < (1 << NUM_TRNG_PREFIX_BITS); ++i) {
		/* ccprintf("bin %u zeros = %u\n", i, global_num_zeros[i]); */
		/* ccprintf("bin %u ones = %u\n", i, global_num_ones[i]); */
		if (global_num_zeros[i] < min_count ||
				global_num_zeros[i] > max_count ||
				global_num_ones[i] < min_count ||
				global_num_ones[i] > max_count)
			passed = false;
	}
	return passed;
}

/*
 * Check that we have enough samples, and that they are healthy.  If the TRNG
 * has not provided healthy samples, keep reading.
 */
static void check_health(void)
{
	read_samples();
	while (!is_healthy()) {
		reset_trng_stats();
		read_samples();
	}
	/* ccprintf("CPRNG initialization succeeded.\n"); */
	global_is_healthy = true;
}

void init_trng(void)
{
#if (!(defined(CONFIG_CUSTOMIZED_RO) && defined(SECTION_IS_RO)))
	/*
	 * Most of the trng initialization requires high permissions. If RO has
	 * dropped the permission level, dont try to read or write these high
	 * permission registers because it will cause rolling reboots. RO
	 * should do the TRNG initialization before dropping the level.
	 */
	if (!runlevel_is_high())
		panic("Tried to initialsed TRNG after lowering run level.\n");
#endif

	/*
	 * TODO(waywardgeek): I need help with this section.  I want to turn off
	 * post-processing, so we get the raw TRNG value each time we read.
	 * That lets us do a simple health check on the LSB.
	 *
	 * I also would like to add a new API to get raw data so the host can
	 * do TRNG health checking.
	 */
	GWRITE(TRNG, POST_PROCESSING_CTRL, 0);
#if 0
	GWRITE(TRNG, POST_PROCESSING_CTRL,
		GC_TRNG_POST_PROCESSING_CTRL_SHUFFLE_BITS_MASK |
		GC_TRNG_POST_PROCESSING_CTRL_CHURN_MODE_MASK);
	GWRITE(TRNG, SLICE_MAX_UPPER_LIMIT, 1);
	GWRITE(TRNG, SLICE_MIN_LOWER_LIMIT, 0);
#endif
	GWRITE(TRNG, TIMEOUT_COUNTER, 0x7ff);
	GWRITE(TRNG, TIMEOUT_MAX_TRY_NUM, 4);
	GWRITE(TRNG, POWER_DOWN_B, 1);
	GWRITE(TRNG, GO_EVENT, 1);

	/* Reset the TRNG health stats. */
	reset_trng_stats();
	global_total_samples = 0;
	memset(global_num_ones, 0,
		(1 << NUM_TRNG_PREFIX_BITS) * sizeof(uint16_t));
	memset(global_num_zeros, 0,
		(1 << NUM_TRNG_PREFIX_BITS) * sizeof(uint16_t));

	/* Gather entropy until we pass a simple statistical test. */
	check_health();
}

void rand_bytes(void *buffer, size_t len)
{
	if (!global_is_healthy)
		panic("Called bmRandBytes before CPRNG initialized\n");
	/*
	 * We could encrypt 0's instead.  It doesn't matter.  Each call to
	 * aes_ctr_enc uses a unique nonce (incremented below).  Note that
	 * DCRYPTO_aes_ctr can have input and output buffers be the same.
	 */
	if (!DCRYPTO_aes_ctr(buffer, (uint8_t *)global_CPRNG_union.state.key,
			KEY_LEN * 8, (uint8_t *)global_CPRNG_union.state.nonce,
			buffer, len))
		panic("AES failed\n");
	/*
	 * It is OK if there is a timing leak on this increment, since the
	 * key and upper 8 bytes of the nonce remain secret.
	 */
	global_CPRNG_union.counter++;
}

uint32_t rand(void)
{
	uint32_t value;

	rand_bytes((uint8_t *)&value, sizeof(uint32_t));
	return value;
}
