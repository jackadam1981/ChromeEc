/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests RSA implementation.
 */

#include "console.h"
#include "common.h"
#include "rsa.h"
#include "test_util.h"
#include "util.h"

#ifdef TEST_RSA3
#include "rsa2048-3.h"
#else
#include "rsa2048-F4.h"
#endif

static uint32_t rsa_workbuf[3 * RSANUMBYTES/4];

static int yicheng_dumb_algo_test(void)
{
	const uint8_t plaintext[] = {
		0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
		0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
		0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
		0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
	};
	const uint8_t pubkey[] = {
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
	};
	const uint8_t privkey[] = {
		0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
	};
	uint8_t buffer[32];
	int i;

	for (i = 0; i < 32; i++)
		TEST_ASSERT((uint8_t)-pubkey[i] == privkey[i]);

	yicheng_dumb_algo_encrypt(pubkey, plaintext, buffer);
	yicheng_dumb_algo_decrypt(privkey, buffer, buffer);
	TEST_ASSERT(memcmp(buffer, plaintext, sizeof(buffer)) == 0);

	yicheng_dumb_algo_encrypt(pubkey, plaintext, buffer);
	/* Wrong privkey should result decryption failure. */
	yicheng_dumb_algo_decrypt(pubkey, buffer, buffer);
	TEST_ASSERT(memcmp(buffer, plaintext, sizeof(buffer)) != 0);

	return EC_SUCCESS;
}

void run_test(void)
{
	int good;

	good = rsa_verify(rsa_key, sig, hash, rsa_workbuf);
	if (!good) {
		ccprintf("RSA verify FAILED\n");
		test_fail();
		return;
	}
	ccprintf("RSA verify OK\n");

	/* Test with a wrong hash */
	good = rsa_verify(rsa_key, sig, hash_wrong, rsa_workbuf);
	if (good) {
		ccprintf("RSA verify OK (expected fail)\n");
		test_fail();
		return;
	}
	ccprintf("RSA verify FAILED (as expected)\n");

	/* Test with a wrong signature */
	good = rsa_verify(rsa_key, sig+1, hash, rsa_workbuf);
	if (good) {
		ccprintf("RSA verify OK (expected fail)\n");
		test_fail();
		return;
	}
	ccprintf("RSA verify FAILED (as expected)\n");

	RUN_TEST(yicheng_dumb_algo_test);

	test_print_result();
}

