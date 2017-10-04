/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test NVRAM
 */

#include "common.h"
#include "console.h"
#include "flash.h"
#include "nvram.h"
#include "system.h"
#include "test_util.h"
#include "util.h"

const int record_size = NVRAM_RECORD_SIZE;

void before_test(void)
{
	nvram_reset();
}

static int test_write(void)
{
	uint8_t d8, size, out;
	int offset;
	int i;

	TEST_ASSERT(nvram_get_cache_status() == DATABLOB_CACHE_INVALID);
	TEST_ASSERT(nvram_get_offset() == 0);
	offset = nvram_flash_offset;

	/* Write first (before initialization). */
	d8 = 0;
	size = sizeof(d8);

	/* Write through to the end of the block. Offset should move along. */
	for (i = 0; i < nvram_block_size / record_size; i++) {
		TEST_ASSERT(nvram_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
		TEST_ASSERT(nvram_get_cache_status() == DATABLOB_CACHE_DIRTY);
		TEST_ASSERT(nvram_get(0, &out, &size) == EC_SUCCESS);
		TEST_EQ(out, d8, "0x%x");
		TEST_EQ((size_t)size, sizeof(d8), "%lu");
		TEST_ASSERT(nvram_write() == EC_SUCCESS);
		TEST_ASSERT(nvram_get_cache_status() == DATABLOB_CACHE_SYNCD);
		TEST_EQ(nvram_get_offset(), offset + record_size, "%d");
		offset = nvram_get_offset();
		d8++;
	}

	/* Write another to wrap around. */
	TEST_ASSERT(nvram_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	TEST_ASSERT(nvram_write() == EC_SUCCESS);
	TEST_EQ(nvram_get_offset(), nvram_flash_offset + record_size, "%d");

	/* Write the same data. Cache stays in SYNCD. Offset shouldn't move. */
	TEST_ASSERT(nvram_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	TEST_ASSERT(nvram_get_cache_status() == DATABLOB_CACHE_SYNCD);
	TEST_ASSERT(nvram_write() == EC_SUCCESS);
	TEST_EQ(nvram_get_offset(), nvram_flash_offset + record_size, "%d");
	offset = nvram_get_offset();

	/* Back-to-back set. Last value should prevail. */
	d8++;
	TEST_ASSERT(nvram_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	d8++;
	TEST_ASSERT(nvram_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	TEST_ASSERT(nvram_get(0, &out, &size) == EC_SUCCESS);
	TEST_EQ(out, d8, "0x%x");
	TEST_ASSERT(nvram_write() == EC_SUCCESS);
	TEST_EQ(nvram_get_offset(), offset + record_size, "%d");

	return EC_SUCCESS;
}

static int test_read(void)
{
	uint8_t size, out;

	/* Read first (before initialization). */
	TEST_ASSERT(nvram_get(0, &out, &size) == EC_ERROR_NOT_FOUND);

	return EC_SUCCESS;
}

static int test_corrupt(void)
{
	uint8_t d8, size;
	uint8_t buf[NVRAM_RECORD_SIZE];

	/*
	 * Corrupted Block.
	 *
	 * Free records should never be followed by a used record. So,
	 * Free|Used|Free|... should never happen. It's treated as corruption.
	 */
	d8 = 0;
	TEST_ASSERT(nvram_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	TEST_ASSERT(nvram_write() == EC_SUCCESS);

	/* Copy a valid record to the next slot. */
	flash_read(nvram_flash_offset, sizeof(buf), buf);
	nvram_reset();
	flash_write(nvram_flash_offset + record_size, sizeof(buf), buf);

	/* Get data from invalid block. */
	TEST_ASSERT(nvram_get(0, &d8, &size) == EC_ERROR_INVAL);

	/* Set data to invalid block. This succeeds. */
	TEST_ASSERT(nvram_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	TEST_ASSERT(nvram_write() == EC_SUCCESS);
	TEST_EQ(nvram_get_offset(), nvram_flash_offset + record_size, "%d");

	return EC_SUCCESS;
}

void run_test(int argc, char **argv)
{
	RUN_TEST(test_write);
	RUN_TEST(test_read);
	RUN_TEST(test_corrupt);

	test_print_result();
}
