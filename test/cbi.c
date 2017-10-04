/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test CBI and alike.
 */

#include "common.h"
#include "console.h"
#include "cros_board_info.h"
#include "flash.h"
#include "gpio.h"
#include "i2c.h"
#include "nvmem.h"
#include "test_util.h"
#include "util.h"

static void reset_cbi(void)
{
	cbi_create();
	cbi_write();
}

static int test_cbi(void)
{
	uint8_t d8;
	uint32_t d32;
	uint8_t b1[32], b2[32], b3[255];
	uint8_t size;
	int out;
	const int tag = 0xff;

	/* Set & get uint8_t */
	reset_cbi();
	d8 = 0xa5;
	TEST_ASSERT(cbi_set_board_info(tag, &d8, sizeof(d8)) == EC_SUCCESS);
	size = 1;
	TEST_ASSERT(cbi_get_board_info(tag, &d8, &size) == EC_SUCCESS);
	TEST_EQ(d8, 0xa5, "0x%x");
	TEST_EQ(size, 1, "%x");
	/* Size-up */
	d32 = 0x1234abcd;
	TEST_ASSERT(cbi_set_board_info(tag, (void *)&d32, sizeof(d32))
		    == EC_SUCCESS);
	size = 4;
	TEST_ASSERT(cbi_get_board_info(tag, (void *)&d32, &size) == EC_SUCCESS);
	TEST_EQ(d32, 0x1234abcd, "0x%x");
	TEST_EQ(size, 4, "%x");

	/* Set & get uint32_t */
	reset_cbi();
	d32 = 0x1234abcd;
	TEST_ASSERT(cbi_set_board_info(tag, (void *)&d32, sizeof(d32))
		    == EC_SUCCESS);
	size = 4;
	TEST_ASSERT(cbi_get_board_info(tag, (void *)&d32, &size) == EC_SUCCESS);
	TEST_EQ(d32, 0x1234abcd, "0x%x");
	TEST_EQ(size, 4, "%x");
	/* Size-down */
	TEST_ASSERT(cbi_set_board_info(tag, &d8, sizeof(d8)) == EC_SUCCESS);
	size = 1;
	TEST_ASSERT(cbi_get_board_info(tag, &d8, &size) == EC_SUCCESS);
	TEST_EQ(d8, 0xa5, "0x%x");
	TEST_EQ(size, 1, "%x");

	/* Set & get string */
	reset_cbi();
	memset(b1, 0xa5, sizeof(b1));
	memset(b2, 0xa5, sizeof(b2));
	TEST_ASSERT(cbi_set_board_info(tag, b1, sizeof(b1)) == EC_SUCCESS);
	size = sizeof(b1);
	TEST_ASSERT(cbi_get_board_info(tag, b1, &size) == EC_SUCCESS);
	TEST_EQ(memcmp(b1, b2, sizeof(b1)), 0, "%d");
	TEST_EQ(size, 32, "%d");
	/* Read buffer too small */
	size = 1;
	TEST_ASSERT(cbi_get_board_info(tag, b1, &size) == EC_ERROR_INVAL);

	/* Data not found */
	reset_cbi();
	size = 1;
	TEST_ASSERT(cbi_get_board_info(tag, &d8, &size) == EC_ERROR_NOT_FOUND);

	/* Data too large */
	reset_cbi();
	memset(b3, 0xa5, sizeof(b3));
	TEST_ASSERT(cbi_set_board_info(tag, b3, sizeof(b3))
		    == EC_ERROR_OVERFLOW);

	/* Populate all data and read out */
	reset_cbi();
	d8 = 0x12;
	TEST_ASSERT(cbi_set_board_info(CBI_TAG_BOARD_VERSION, &d8, sizeof(d8))
		    == EC_SUCCESS);
	TEST_ASSERT(cbi_set_board_info(CBI_TAG_OEM_ID, &d8, sizeof(d8))
		    == EC_SUCCESS);
	TEST_ASSERT(cbi_set_board_info(CBI_TAG_SKU_ID, &d8, sizeof(d8))
		    == EC_SUCCESS);
	TEST_ASSERT(cbi_set_board_info(CBI_TAG_MODEL_ID, &d8, sizeof(d8))
		    == EC_SUCCESS);
	TEST_ASSERT(cbi_set_board_info(CBI_TAG_FW_CONFIG, &d8, sizeof(d8))
		    == EC_SUCCESS);
	TEST_ASSERT(cbi_set_board_info(CBI_TAG_PCB_SUPPLIER, &d8, sizeof(d8))
		    == EC_SUCCESS);
	TEST_ASSERT(cbi_get_board_version(&d32) == EC_SUCCESS);
	TEST_ASSERT(d32 == d8);
	TEST_ASSERT(cbi_get_oem_id(&d32) == EC_SUCCESS);
	TEST_ASSERT(d32 == d8);
	TEST_ASSERT(cbi_get_sku_id(&d32) == EC_SUCCESS);
	TEST_ASSERT(d32 == d8);
	TEST_ASSERT(cbi_get_model_id(&d32) == EC_SUCCESS);
	TEST_ASSERT(d32 == d8);
	TEST_ASSERT(cbi_get_fw_config(&d32) == EC_SUCCESS);
	TEST_ASSERT(d32 == d8);
	TEST_ASSERT(cbi_get_pcb_supplier(&d32) == EC_SUCCESS);
	TEST_ASSERT(d32 == d8);

	/* Write protect */
	gpio_set_level(GPIO_WP, 1);
	TEST_ASSERT(cbi_write() == EC_ERROR_ACCESS_DENIED);

	/* Bad CRC */
	reset_cbi();
	d8 = 0xa5;
	TEST_ASSERT(cbi_set_board_info(tag, &d8, sizeof(d8)) == EC_SUCCESS);
	i2c_read8(I2C_PORT_EEPROM, I2C_ADDR_EEPROM_FLAGS,
		   offsetof(struct datablob_header, crc), &out);
	i2c_write8(I2C_PORT_EEPROM, I2C_ADDR_EEPROM_FLAGS,
		   offsetof(struct datablob_header, crc), ++out);
	cbi_invalidate_cache();
	TEST_ASSERT(cbi_get_board_info(tag, &d8, &size) == EC_ERROR_INVAL);

	return EC_SUCCESS;
}

const int record_size = NVMEM_RECORD_SIZE;

static int test_nvmem(void)
{
	uint8_t d8, size, out;
	uint8_t buf[NVMEM_RECORD_SIZE];
	int offset;
	int i;

	nvmem_reset();
	TEST_ASSERT(nvmem_get_cache_status() == DATABLOB_CACHE_INVALID);
	TEST_ASSERT(nvmem_get_offset() == 0);
	offset = nvmem_flash_offset;

	/* Write first (before initialization). */
	d8 = 0;
	size = sizeof(d8);
	/* Write through to the end of the block. Offset should move along. */
	for (i = 0; i < nvmem_block_size / record_size; i++) {
		TEST_ASSERT(nvmem_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
		TEST_ASSERT(nvmem_get_cache_status() == DATABLOB_CACHE_DIRTY);
		TEST_ASSERT(nvmem_get(0, &out, &size) == EC_SUCCESS);
		TEST_ASSERT(out == d8);
		TEST_ASSERT(size == sizeof(d8));
		TEST_ASSERT(nvmem_write() == EC_SUCCESS);
		TEST_ASSERT(nvmem_get_cache_status() == DATABLOB_CACHE_SYNCD);
		TEST_ASSERT(nvmem_get_offset() == offset + record_size);
		offset = nvmem_get_offset();
		d8++;
	}
	/* Write another to wrap around. */
	TEST_ASSERT(nvmem_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	TEST_ASSERT(nvmem_write() == EC_SUCCESS);
	TEST_ASSERT(nvmem_get_offset() == nvmem_flash_offset + record_size);
	/* Write the same data. Cache stays in SYNCD. Offset shouldn't move. */
	TEST_ASSERT(nvmem_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	TEST_ASSERT(nvmem_get_cache_status() == DATABLOB_CACHE_SYNCD);
	TEST_ASSERT(nvmem_write() == EC_SUCCESS);
	TEST_ASSERT(nvmem_get_offset() == nvmem_flash_offset + record_size);
	offset = nvmem_get_offset();
	/* Back-to-back set. Last value should prevail. */
	d8++;
	TEST_ASSERT(nvmem_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	d8++;
	TEST_ASSERT(nvmem_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	TEST_ASSERT(nvmem_get(0, &out, &size) == EC_SUCCESS);
	TEST_ASSERT(out == d8);
	TEST_ASSERT(nvmem_write() == EC_SUCCESS);
	TEST_ASSERT(nvmem_get_offset() == offset + record_size);

	/* Read first (before initialization). */
	nvmem_reset();
	TEST_ASSERT(nvmem_get(0, &out, &size) == EC_ERROR_NOT_FOUND);

	/*
	 * Corrupted Block.
	 *
	 * Free records should never be followed by a used record. So,
	 * Free|Used|Free|... should never happen. It's treated as corruption.
	 */
	nvmem_reset();
	d8 = 0;
	TEST_ASSERT(nvmem_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	TEST_ASSERT(nvmem_write() == EC_SUCCESS);
	/* Copy a valid record to the next slot. */
	flash_read(nvmem_flash_offset, sizeof(buf), buf);
	nvmem_reset();
	flash_write(nvmem_flash_offset + record_size, sizeof(buf), buf);
	/* Get data from invalid block. */
	TEST_ASSERT(nvmem_get(0, &d8, &size) == EC_ERROR_INVAL);
	/* Set data to invalid block. This succeeds. */
	TEST_ASSERT(nvmem_set(0, &d8, sizeof(d8)) == EC_SUCCESS);
	TEST_ASSERT(nvmem_write() == EC_SUCCESS);
	TEST_ASSERT(nvmem_get_offset() == nvmem_flash_offset + record_size);

	return EC_SUCCESS;
}

void run_test(int argc, char **argv)
{
	RUN_TEST(test_cbi);
	RUN_TEST(test_nvmem);

	test_print_result();
}
