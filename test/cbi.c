/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test CBI and alike.
 */

#include "common.h"
#include "console.h"
#include "cros_board_info.h"
#include "gpio.h"
#include "i2c.h"
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
	TEST_ASSERT(cbi_get_board_info(tag, &d8, &size) == EC_ERROR_UNKNOWN);

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
		   offsetof(struct cbi_header, crc), &out);
	i2c_write8(I2C_PORT_EEPROM, I2C_ADDR_EEPROM_FLAGS,
		   offsetof(struct cbi_header, crc), ++out);
	cbi_invalidate_cache();
	TEST_ASSERT(cbi_get_board_info(tag, &d8, &size) == EC_ERROR_UNKNOWN);

	return EC_SUCCESS;
}

void run_test(int argc, char **argv)
{
	RUN_TEST(test_cbi);

	test_print_result();
}
