/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "cryptoc/util.h"
#include "dcrypto.h"
#include "extension.h"
#include "trng.h"


/**
 * TRNG FIFO size is computed from minimal assessed entropy H >=6.4
 * security_level = 256 bits
 * as trunc(((security_level * bits_in_byte/ H) + 31) / bits_in_uint32)
 */
#define CR50_TRNG_FIFO_SIZE 10

uint32_t get_entropy32(void)
{
	static uint32_t entropy_fifo[SHA256_DIGEST_WORDS];
	static size_t fifo_left;
	uint32_t out;

	/* TODO (sukhomlinov): add FIPS TRNG health tests */
	if (!fifo_left) {
		uint32_t trng_fifo[CR50_TRNG_FIFO_SIZE];
		/* read enough raw entropy */
		rand_bytes(trng_fifo, sizeof(trng_fifo));
		DCRYPTO_SHA256_hash(trng_fifo, sizeof(trng_fifo),
				    (uint8_t *)entropy_fifo);
		fifo_left = SHA256_DIGEST_WORDS;
	}
	out = entropy_fifo[fifo_left - 1];
	/* wipe entropy as soon as it's used */
	entropy_fifo[--fifo_left] = 0;
	return out;
}

void get_entropy(void *buffer, size_t len)
{
	int random_togo = 0;
	int buffer_index = 0;
	uint32_t random_value;
	uint8_t *buf = (uint8_t *)buffer;

	/*
	 * Retrieve random numbers in 4 byte quantities and pack as many bytes
	 * as needed into 'buffer'. If len is not divisible by 4, the
	 * remaining random bytes get dropped.
	 */
	while (buffer_index < len) {
		if (!random_togo) {
			random_value = get_entropy32();
			random_togo = sizeof(random_value);
		}
		buf[buffer_index++] = random_value >> ((random_togo-- - 1) * 8);
	}
}

/* Since raw TRNG input shouldn't be used as random number generator,
 * implement FIPS-compliant CR50-wide DRBG for same purpose.
 */
static struct drbg_ctx fips_drbg;

/* if zero means cr50_drbg need initialization */
static int fips_drbg_init_flag;

/* should be called on board init */
void fips_drbg_init_clear(void)
{
	fips_drbg_init_flag = 0;
}

void fips_drbg_init(void)
{
	uint32_t x[(440 + 31) / 32];
	/* initialize DRBG with 440 bits of entropy as required
	 * by NIST SP 800-90A 10.1. Includes entropy and nonce,
	 * both received from entropy source.
	 */
	get_entropy(x, sizeof(x));
	hmac_drbg_init(&fips_drbg, &x, sizeof(x), NULL, 0, NULL, 0);
	always_memset(x, 0, sizeof(x));
	fips_drbg_init_flag = 1;
}

void fips_rand_bytes(void *buffer, size_t len)
{
	int err;
	/**
	 * make sure cr50 DRBG is initialized after power-on or resume,
	 * but do it on first use to minimize latency of board_init()
	 */
	if (!fips_drbg_init_flag)
		fips_drbg_init();

	err = hmac_drbg_generate(&fips_drbg, buffer, len, NULL, 0);
	/**
	 *  if reseed is required, do it. handle future health tests
	 */
	while (err) {
		/* minimal entropy would be equal security strength */
		uint32_t entropy_input[SHA256_DIGEST_WORDS];

		get_entropy(&entropy_input, sizeof(entropy_input));
		hmac_drbg_reseed(&fips_drbg, entropy_input,
				 sizeof(entropy_input), NULL, 0, NULL, 0);
		always_memset(entropy_input, 0, sizeof(entropy_input));
		err = hmac_drbg_generate(&fips_drbg, &buffer, sizeof(len), NULL,
					 0);
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
	default:
		return VENDOR_RC_BOGUS_ARGS;
	}
	*response_size = text_len;
	return VENDOR_RC_SUCCESS;
}

DECLARE_VENDOR_COMMAND(VENDOR_CC_TRNG_TEST, trng_test);

#endif   /* CRYPTO_TEST_SETUP */
