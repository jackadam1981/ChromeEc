/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test battery info in CBI
 */

#include "battery_fuel_gauge.h"
#include "common.h"
#include "console.h"
#include "cros_board_info.h"
#include "ec_commands.h"
#include "test_util.h"
#include "util.h"
#include "write_protect.h"

void batt_conf_main(void);
extern struct board_batt_params default_battery_conf;
extern struct board_batt_params *battery_conf;
extern char manuf_name[32];
extern char device_name[32];

const struct board_batt_params board_battery_info[] = {
	[BATTERY_C214] = {
		.fuel_gauge = {
			.manuf_name = "AS1GUXd3KB",
			.device_name = "C214-43",
			.ship_mode = {
				.reg_addr = 0x0,
				.reg_data = { 0x10, 0x10 },
			},
			.fet = {
				.reg_addr = 0x00,
				.reg_mask = 0x2000,
				.disconnect_val = 0x2000,
			},
			.flags = FUEL_GAUGE_FLAG_MFGACC,
		},
		.batt_info = {
			.voltage_max = 13200,
			.voltage_normal = 11550,
			.voltage_min = 9000,
			.precharge_current = 256,
			.start_charging_min_c = 0,
			.start_charging_max_c = 45,
			.charging_min_c = 0,
			.discharging_min_c = 0,
			.discharging_max_c = 60,
		},
	},
};

static struct board_batt_params conf_in_cbi = {
	.fuel_gauge = {
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

int battery_manufacturer_name(char *dest, int size)
{
	strncpy(dest, "AS1GUXd3KB", sizeof("AS1GUXd3KB"));
	return EC_SUCCESS;
}

int battery_device_name(char *dest, int size)
{
	strncpy(dest, "C214-43", sizeof("C214-43"));
	return EC_SUCCESS;
}

const enum battery_type DEFAULT_BATTERY_TYPE = BATTERY_C214;

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

static union ec_common_control mock_common_control;
static int cbi_get_common_control_return;

int cbi_get_common_control(union ec_common_control *ctrl)
{
	*ctrl = mock_common_control;

	return cbi_get_common_control_return;
}

void batt_conf_dump(const struct board_batt_params *info)
{
	const struct fuel_gauge_info *fg = &info->fuel_gauge;
	const struct ship_mode_info *ship = &info->fuel_gauge.ship_mode;
	const struct sleep_mode_info *sleep = &info->fuel_gauge.sleep_mode;
	const struct fet_info *fet = &info->fuel_gauge.fet;
	const struct battery_info *batt = &info->batt_info;

	ccprintf(".fuel_gauge = {\n");

	ccprintf("\t.manuf_name = \"%s\",\n", fg->manuf_name);
	ccprintf("\t.device_name= \"%s\",\n", fg->device_name);
	ccprintf("\t.flags = 0x%x,\n", fg->flags);

	ccprintf("\t.ship_mode = {\n");
	ccprintf("\t\t.reg_addr = 0x%02x,\n", ship->reg_addr);
	ccprintf("\t\t.reg_data = { 0x%04x, 0x%04x },\n", ship->reg_data[0],
		 ship->reg_data[1]);
	ccprintf("\t},\n");

	ccprintf("\t.sleep_mode = {\n");
	ccprintf("\t\t.reg_addr = 0x%02x,\n", sleep->reg_addr);
	ccprintf("\t\t.reg_data = 0x%04x,\n", sleep->reg_data);
	ccprintf("\t},\n");

	ccprintf("\t.fet = {\n");
	ccprintf("\t\t.reg_addr = 0x%02x,\n", fet->reg_addr);
	ccprintf("\t\t.reg_mask = 0x%04x,\n", fet->reg_mask);
	ccprintf("\t\t.disconnect_val = 0x%x,\n", fet->disconnect_val);
	ccprintf("\t\t.cfet_mask = 0x%04x,\n", fet->cfet_mask);
	ccprintf("\t\t.cfet_off_val = 0x%04x,\n", fet->cfet_off_val);
	ccprintf("\t},\n");

	ccprintf("},\n"); /* end of fuel_gauge */

	ccprintf(".batt_info = {\n");
	ccprintf("\t.voltage_max = %d,\n", batt->voltage_max);
	ccprintf("\t.voltage_normal = %d,\n", batt->voltage_normal);
	ccprintf("\t.voltage_min = %d,\n", batt->voltage_min);
	ccprintf("\t.precharge_voltage= %d,\n", batt->precharge_voltage);
	ccprintf("\t.precharge_current = %d,\n", batt->precharge_current);
	ccprintf("\t.start_charging_min_c = %d,\n", batt->start_charging_min_c);
	ccprintf("\t.start_charging_max_c = %d,\n", batt->start_charging_max_c);
	ccprintf("\t.charging_min_c = %d,\n", batt->charging_min_c);
	ccprintf("\t.charging_max_c = %d,\n", batt->charging_max_c);
	ccprintf("\t.discharging_min_c = %d,\n", batt->discharging_min_c);
	ccprintf("\t.discharging_max_c = %d,\n", batt->discharging_max_c);
	ccprintf("},\n"); /* end of batt_info */
}

DECLARE_EC_TEST(test_batt_conf_main)
{
	mock_common_control.bcic_enabled = 1;
	cbi_get_common_control_return = EC_SUCCESS;
	uint8_t buf[sizeof(struct batt_conf_header) +
		    sizeof(struct board_batt_params)];
	struct batt_conf_header *head = (struct batt_conf_header *)buf;

	/* On POR, no config in CBI. Legacy mode should fall back to [0]. */
	zassert_equal(memcmp(battery_conf, &board_battery_info[0],
			     sizeof(*battery_conf)),
		      0);

	memset(&default_battery_conf, 0, sizeof(default_battery_conf));

	ccprintf("total size=%lu (size of conf = %lu)\n", sizeof(buf),
		 sizeof(struct board_batt_params));

	/*
	 * manuf_name != manuf_name
	 */
	ccprintf("manuf_name != manuf_name\n");
	head->struct_version = 0;
	strncpy(head->manuf_name, "foo", sizeof("foo"));
	memset(head->device_name, 0, sizeof(head->device_name));
	memcpy(head->data, &conf_in_cbi, sizeof(conf_in_cbi));
	cbi_set_board_info(CBI_TAG_BATTERY_CONFIG, buf, sizeof(buf));
	/* Run detection */
	batt_conf_main();
	conf_in_cbi.fuel_gauge.manuf_name = NULL;
	conf_in_cbi.fuel_gauge.device_name = NULL;
	zassert_not_equal(memcmp(&default_battery_conf, &conf_in_cbi,
				 sizeof(default_battery_conf)),
			  0);

	memset(&default_battery_conf, 0, sizeof(default_battery_conf));

	/*
	 * manuf_name == manuf_name && device_name == ""
	 */
	ccprintf("manuf_name == manuf_name && device_name == \"\"\n");
	strncpy(head->manuf_name, "AS1GUXd3KB", sizeof("AS1GUXd3KB"));
	cbi_set_board_info(CBI_TAG_BATTERY_CONFIG, buf, sizeof(buf));
	/* Detection fails due to manuf_name mismatch. */
	batt_conf_main();
	conf_in_cbi.fuel_gauge.manuf_name = manuf_name;
	conf_in_cbi.fuel_gauge.device_name = NULL;
	zassert_equal(memcmp(&default_battery_conf, &conf_in_cbi,
			     sizeof(default_battery_conf)),
		      0);

	memset(&default_battery_conf, 0, sizeof(default_battery_conf));

	/*
	 * manuf_name == manuf_name && device_name != device_name
	 */
	ccprintf("manuf_name == manuf_name && device_name != device_name\n");
	strncpy(head->device_name, "foo", sizeof("foo"));
	cbi_set_board_info(CBI_TAG_BATTERY_CONFIG, buf, sizeof(buf));
	/* Detection fails due to batt_name mismatch. */
	batt_conf_main();
	conf_in_cbi.fuel_gauge.manuf_name = NULL;
	conf_in_cbi.fuel_gauge.device_name = NULL;
	zassert_not_equal(memcmp(&default_battery_conf, &conf_in_cbi,
				 sizeof(default_battery_conf)),
			  0);

	memset(&default_battery_conf, 0, sizeof(default_battery_conf));

	/*
	 * manuf_name == manuf_name && device_name == device_name
	 */
	strncpy(head->device_name, "C214-43", sizeof("C214-43"));
	cbi_set_board_info(CBI_TAG_BATTERY_CONFIG, buf, sizeof(buf));
	/* Detection fails due to batt_name mismatch. */
	batt_conf_main();
	conf_in_cbi.fuel_gauge.manuf_name = manuf_name;
	conf_in_cbi.fuel_gauge.device_name = device_name;
	zassert_equal(memcmp(&default_battery_conf, &conf_in_cbi,
			     sizeof(default_battery_conf)),
		      0);

	return EC_SUCCESS;
}

TEST_SUITE(test_suite_battery_config)
{
	ztest_test_suite(test_battery_config,
			 ztest_unit_test_setup_teardown(test_batt_conf_main,
							test_setup,
							test_teardown));
	ztest_run_test_suite(test_battery_config);
}
