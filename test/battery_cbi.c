/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test battery info in CBI
 */

#include "common.h"
#include "console.h"
#include "cros_board_info.h"
#include "battery_fuel_gauge.h"
#include "ec_commands.h"
#include "test_util.h"
#include "util.h"
#include "write_protect.h"

int batt_cbi_read_ship_mode(void);
int batt_cbi_read_sleep_mode(void);

struct board_batt_params default_battery_info = {};

struct board_batt_params reference_info = {
	.fuel_gauge = {
		.manuf_name = { 'x', 'y', 'z' },
		.ship_mode = {
			.reg_addr = 0xaa,
			.reg_data = {
				[0] = 0x89ab,
				[1] = 0xcdef,
			},
		},
	},
	.batt_info = {
		.voltage_max = 8400,
		.voltage_normal = 7400,
		.voltage_min = 6000,
		.precharge_current = 64, /* mA */
		.start_charging_min_c = 0,
		.start_charging_max_c = 50,
		.charging_min_c = 0,
		.charging_max_c = 50,
		.discharging_min_c = -20,
		.discharging_max_c = 60,
	},
};

static void test_setup(void)
{
	/* Make sure that write protect is disabled */
	write_protect_set(0);

	cbi_create();
	cbi_write();
}

static void test_teardown(void)
{
}

DECLARE_EC_TEST(test_read_ship_mode)
{
	uint8_t d8;
	enum cbi_data_tag tag;
	struct ship_mode_info *battery_info =
			&reference_info.fuel_gauge.ship_mode;
	struct ship_mode_info *default_info =
			&default_battery_info.fuel_gauge.ship_mode;

	tag = CBI_TAG_BATT_SHIP_MODE_FLAGS;
	d8 = BIT(0);
	zassert_equal(cbi_set_board_info(tag, &d8, sizeof(d8)), EC_SUCCESS);
	tag = CBI_TAG_BATT_SHIP_MODE_REG_ADDR;
	zassert_equal(cbi_set_board_info(tag, &battery_info->reg_addr,
					 sizeof(battery_info->reg_addr)),
		      EC_SUCCESS);
	tag = CBI_TAG_BATT_SHIP_MODE_REG_DATA;
	zassert_equal(cbi_set_board_info(tag, (uint8_t *)&battery_info->reg_data,
					 sizeof(battery_info->reg_data)),
		      EC_SUCCESS);

	/* Read */
	zassert_equal(batt_cbi_read_ship_mode(), EC_SUCCESS);

	zassert_equal(default_info->reg_addr, battery_info->reg_addr);
	zassert_equal(default_info->reg_data[0], battery_info->reg_data[0]);
	zassert_equal(default_info->reg_data[1], battery_info->reg_data[1]);
	zassert_equal(default_info->wb_support, 1);

	/*
	zassert_equal(d8, 0xa5, "0x%x, 0x%x", d8, 0xa5);
	zassert_equal(size, 1, "%x, %x", size, 1);
	*/

	return EC_SUCCESS;
}

DECLARE_EC_TEST(test_read_sleep_mode)
{
	uint8_t d8;
	enum cbi_data_tag tag;
	struct sleep_mode_info *battery_info =
			&reference_info.fuel_gauge.sleep_mode;
	struct sleep_mode_info *default_info =
			&default_battery_info.fuel_gauge.sleep_mode;

	tag = CBI_TAG_BATT_SLEEP_MODE_FLAGS;
	d8 = BIT(0);
	zassert_equal(cbi_set_board_info(tag, &d8, sizeof(d8)), EC_SUCCESS);
	tag = CBI_TAG_BATT_SLEEP_MODE_REG_ADDR;
	zassert_equal(cbi_set_board_info(tag, &battery_info->reg_addr,
					 sizeof(battery_info->reg_addr)),
		      EC_SUCCESS);
	tag = CBI_TAG_BATT_SLEEP_MODE_REG_DATA;
	zassert_equal(cbi_set_board_info(tag, (uint8_t *)&battery_info->reg_data,
					 sizeof(battery_info->reg_data)),
		      EC_SUCCESS);

	/* Read */
	zassert_equal(batt_cbi_read_sleep_mode(), EC_SUCCESS);

	zassert_equal(default_info->reg_addr, battery_info->reg_addr);
	zassert_equal(default_info->reg_data, battery_info->reg_data);
	zassert_equal(default_info->sleep_supported, 1);

	return EC_SUCCESS;
}

TEST_SUITE(test_suite_battery_cbi)
{
	ztest_test_suite(
		test_battery_cbi,
		ztest_unit_test_setup_teardown(test_read_ship_mode,
					       test_setup, test_teardown),
		ztest_unit_test_setup_teardown(test_read_sleep_mode,
					       test_setup, test_teardown)
					       );
	ztest_run_test_suite(test_battery_cbi);
}
