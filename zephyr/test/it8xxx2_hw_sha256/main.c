/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sha256.h>
#include <zephyr/ztest_assert.h>
#include <zephyr/ztest_test_new.h>

static const uint8_t sha256_input[] = { 0xaa, 0xaa, 0x55, 0x55 };
static const uint8_t sha256_output[SHA256_DIGEST_SIZE] = {
	0xd1, 0x25, 0x79, 0xe0, 0x37, 0x88, 0x89, 0x6e, 0xdc, 0xa0, 0xef,
	0x74, 0x6e, 0xb1, 0x64, 0xc7, 0x03, 0x19, 0x95, 0x6e, 0x62, 0x1f,
	0x32, 0x67, 0x0d, 0x6e, 0xff, 0xaa, 0xf7, 0xb0, 0x7d, 0x1a
};

ZTEST_SUITE(it8xxx2_hw_sha256_driver, NULL, NULL, NULL, NULL, NULL);

ZTEST(it8xxx2_hw_sha256_driver, test_it8xxx2_hw_sha256)
{
	static struct sha256_ctx ctx;
	uint8_t *hash;

	SHA256_init(&ctx);
	SHA256_update(&ctx, sha256_input, sizeof(sha256_input));
	hash = SHA256_final(&ctx);
	zassert_mem_equal(hash, sha256_output, SHA256_DIGEST_SIZE, NULL);
}
