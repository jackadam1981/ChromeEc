/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests SHA256 implementation.
 */

#include "console.h"
#include "common.h"
#include "sha256.h"
#include "test_util.h"
#include "util.h"

static const uint8_t *input = "The quick brown fox jumps over the lazy dog";
static const uint8_t sha256_output[] = {
	0xd7, 0xa8, 0xfb, 0xb3, 0x07, 0xd7, 0x80, 0x94, 0x69, 0xca, 0x9a, 0xbc,
	0xb0, 0x08, 0x2e, 0x4f, 0x8d, 0x56, 0x51, 0xe4, 0x6d, 0x3c, 0xdb, 0x76,
	0x2d, 0x02, 0xd0, 0xbf, 0x37, 0xc9, 0xe5, 0x92
};
BUILD_ASSERT(sizeof(sha256_output) == SHA256_DIGEST_SIZE);

/* HMAC test */
static const uint8_t *key = "key";
static const uint8_t hmac_output[] = {
	0xf7, 0xbc, 0x83, 0xf4, 0x30, 0x53, 0x84, 0x24, 0xb1, 0x32, 0x98, 0xe6,
	0xaa, 0x6f, 0xb1, 0x43, 0xef, 0x4d, 0x59, 0xa1, 0x49, 0x46, 0x17, 0x59,
	0x97, 0x47, 0x9d, 0xbc, 0x2d, 0x1a, 0x3c, 0xd8
};
BUILD_ASSERT(sizeof(hmac_output) == SHA256_DIGEST_SIZE);

static int test_sha256(void)
{
	struct sha256_ctx ctx;
	uint8_t *tmp;
	int input_len = strlen(input);
	int i;

	/* Basic test */
	SHA256_init(&ctx);
	SHA256_update(&ctx, input, input_len);
	tmp = SHA256_final(&ctx);

	if (memcmp(tmp, sha256_output, sizeof(sha256_output)) != 0) {
		ccprintf("SHA256 test failed\n");
		return 0;
	}

	/* Splitting the input string in chunks of 1 byte also works. */
	SHA256_init(&ctx);
	for (i = 0; i < input_len; i++)
		SHA256_update(&ctx, &input[i], 1);
	tmp = SHA256_final(&ctx);

	if (memcmp(tmp, sha256_output, sizeof(sha256_output)) != 0) {
		ccprintf("SHA256 test failed (1-byte chunks)\n");
		return 0;
	}

	return 1;
}

static int test_hmac(void)
{
	uint8_t output[SHA256_DIGEST_SIZE];
	int key_len = strlen(key);
	int input_len = strlen(input);

	hmac_SHA256(output, key, key_len, input, input_len);

	if (memcmp(output, hmac_output, sizeof(hmac_output)) != 0) {
		ccprintf("hmac_SHA256 test failed\n");
		return 0;
	}

	return 1;
}

void run_test(void)
{
	if (!test_sha256()) {
		test_fail();
		return;
	}

	if (!test_hmac()) {
		test_fail();
		return;
	}

	test_pass();
}
