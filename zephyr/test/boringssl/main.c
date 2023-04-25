/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <zephyr/ztest_assert.h>
#include <zephyr/ztest_test_new.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>

ZTEST_SUITE(boringssl_crypto, NULL, NULL, NULL, NULL, NULL);

ZTEST(boringssl_crypto, test_boringssl_self_test)
{
	zassert_equal(BORINGSSL_self_test(), 1, "BoringSSL self-test failed");
}

ZTEST(boringssl_crypto, test_rand)
{
	uint8_t zero[256] = { 0 };
	uint8_t buf1[256];
	uint8_t buf2[256];

	RAND_bytes(buf1, sizeof(buf1));
	RAND_bytes(buf2, sizeof(buf2));

	zassert_true(memcmp(buf1, zero, sizeof(zero)) != 0);
	zassert_true(memcmp(buf2, zero, sizeof(zero)) != 0);
	zassert_true(memcmp(buf1, buf2, sizeof(buf1)) != 0);
}
