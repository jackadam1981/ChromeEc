/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Fuzzer for new_nvmem implementation.
 */

#include "test/nvmem_test.h"

#include "console.h"
#include "new_nvmem.h"
#include "crc.h"

int app_cipher(const void *salt_p, void *out_p, const void *in_p, size_t size)
{

	const uint8_t *in = in_p;
	uint8_t *out = out_p;
	const uint8_t *salt = salt_p;
	size_t i;

	for (i = 0; i < size; i++)
		out[i] = in[i] ^ salt[i % CIPHER_SALT_SIZE];

	return 1;
}

void app_compute_hash(uint8_t *p_buf, size_t num_bytes,
		      uint8_t *p_hash, size_t hash_bytes)
{
	uint32_t crc;
	uint32_t *p_data;
	int n;
	size_t tail_size;

	crc32_init();
	/* Assuming here that buffer is 4 byte aligned. */
	p_data = (uint32_t *)p_buf;
	for (n = 0; n < num_bytes / 4; n++)
		crc32_hash32(*p_data++);

	tail_size = num_bytes % 4;
	if (tail_size) {
		uint32_t tail;

		tail = 0;
		memcpy(&tail, p_data, tail_size);
		crc32_hash32(tail);
	}

	/*
	 * Crc32 of 0xffffffff is 0xffffffff. Let's spike the results to avoid
	 * this unfortunate Crc32 property.
	 */
	crc = crc32_result() ^ 0x55555555;

	for (n = 0; n < hash_bytes; n += sizeof(crc)) {
		size_t copy_bytes = MIN(sizeof(crc), hash_bytes - n);

		memcpy(p_hash + n, &crc, copy_bytes);
	}
}

int crypto_enabled(void)
{
	return 1;
}

int DCRYPTO_ladder_is_enabled(void)
{
	return 1;
}

void nvmem_wipe_cache(void)
{
}

void flash_log_add_event(uint8_t type, uint8_t size, void *payload)
{
}

void run_test(void) {}

int test_fuzz_one_input(const uint8_t *data, unsigned int size)
{
	ccprintf("hello\n");
	/* ASSERT(new_nvmem_init() == EC_SUCCESS); */
	return 0;
}
