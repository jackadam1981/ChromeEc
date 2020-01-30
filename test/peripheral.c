/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test backlight control based on lid
 */


#include "battery.h"
#include "host_command.h"
#include "test_util.h"

/* Mock out dependencies needed by smart battery */
int i2c_read16(const int port, const uint16_t slave_addr_flags, int offset,
	       int *data)
{
	return EC_SUCCESS;
}

int i2c_write16(const int port, const uint16_t slave_addr_flags, int offset,
		int data)
{
	return EC_SUCCESS;
}

int i2c_read_string(const int port, const uint16_t slave_addr_flags, int offset,
		    uint8_t *data, int len)
{
	return EC_SUCCESS;
}

int i2c_write_block(const int port, const uint16_t slave_addr_flags, int offset,
		    const uint8_t *data, int len)
{
	return EC_SUCCESS;
}

void battery_compensate_params(struct batt_params *batt)
{
}

void board_battery_compensate_params(struct batt_params *batt)
{
}
/* End dependency mock for smart battery */

static int test_smart_battery_good(void)
{
	struct ec_params_locate_chip params = {
		.type = EC_CHIP_TYPE_SMART_BATTERY,
		.index = 0,
	};
	struct ec_response_locate_chip resp;
	int rv;

	rv = test_send_host_command(EC_CMD_LOCATE_CHIP, 0, &params,
				    sizeof(params), &resp, sizeof(resp));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");
	TEST_EQ(resp.bus_type, EC_BUS_TYPE_I2C, "%d");
	/* Value of 31 is defined in test_config.h for battery port */
	TEST_EQ(resp.i2c_info.port, 31, "%d");
	TEST_EQ(resp.i2c_info.addr_flags, 0x0b, "0x%02x");

	return EC_SUCCESS;
}

static int test_smart_battery_bad_index(void)
{
	struct ec_params_locate_chip params = {
		.type = EC_CHIP_TYPE_SMART_BATTERY,
		.index = 1,
	};
	struct ec_response_locate_chip resp;
	int rv;

	rv = test_send_host_command(EC_CMD_LOCATE_CHIP, 0, &params,
				    sizeof(params), &resp, sizeof(resp));

	TEST_EQ(rv, EC_RES_OVERFLOW, "%d");

	return EC_SUCCESS;
}

static int test_smart_battery_not_present(void)
{
	struct ec_params_locate_chip params = {
		.type = EC_CHIP_TYPE_SMART_BATTERY,
		.index = 0,
	};
	struct ec_response_locate_chip resp;
	int rv;

	rv = test_send_host_command(EC_CMD_LOCATE_CHIP, 0, &params,
				    sizeof(params), &resp, sizeof(resp));

	TEST_EQ(rv, EC_RES_UNAVAILABLE, "%d");

	return EC_SUCCESS;
}

static int test_invalid_chip(void)
{
	struct ec_params_locate_chip params = {
		.type = EC_CHIP_TYPE_COUNT, /* Count is one more than valid */
	};
	struct ec_response_locate_chip resp;
	int rv;

	rv = test_send_host_command(EC_CMD_LOCATE_CHIP, 0, &params,
				    sizeof(params), &resp, sizeof(resp));

	TEST_EQ(rv, EC_RES_INVALID_PARAM, "%d");

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_invalid_chip);

	if (IS_ENABLED(CONFIG_BATTERY_SMART)) {
		RUN_TEST(test_smart_battery_good);
		RUN_TEST(test_smart_battery_bad_index);
	} else {
		RUN_TEST(test_smart_battery_not_present);
	}

	test_print_result();
}
