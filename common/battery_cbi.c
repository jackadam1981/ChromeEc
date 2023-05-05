/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Library for read/write battery info in CBI
 */

#include "battery_fuel_gauge.h"
#include "console.h"
#include "cros_board_info.h"
#include "hooks.h"

test_export_static int batt_cbi_read_ship_mode(void)
{
	struct ship_mode_info *info =
			&default_battery_info.fuel_gauge.ship_mode;
	uint8_t d8;
	uint8_t size;
	int rv;

	size = sizeof(info->reg_addr);
	rv = cbi_get_board_info(CBI_TAG_BATT_SHIP_MODE_REG_ADDR,
				&info->reg_addr, &size);
	if (rv)
		return rv;

	size = sizeof(info->reg_data);
	rv = cbi_get_board_info(CBI_TAG_BATT_SHIP_MODE_REG_DATA,
				(uint8_t *)&info->reg_data, &size);
	if (rv)
		return rv;

	size = sizeof(d8);
	rv = cbi_get_board_info(CBI_TAG_BATT_SHIP_MODE_FLAGS, &d8, &size);
	if (rv)
		return rv;
	info->wb_support = d8 & BIT(0) ? 1 : 0;

	return EC_SUCCESS;
}

test_export_static int batt_cbi_read_sleep_mode(void)
{
	struct sleep_mode_info *info =
			&default_battery_info.fuel_gauge.sleep_mode;
	uint8_t d8;
	uint8_t size;
	int rv;

	size = sizeof(info->reg_addr);
	rv = cbi_get_board_info(CBI_TAG_BATT_SLEEP_MODE_REG_ADDR,
				&info->reg_addr, &size);
	if (rv)
		return rv;

	size = sizeof(info->reg_data);
	rv = cbi_get_board_info(CBI_TAG_BATT_SLEEP_MODE_REG_DATA,
				(uint8_t *)&info->reg_data, &size);
	if (rv)
		return rv;

	size = sizeof(d8);
	rv = cbi_get_board_info(CBI_TAG_BATT_SLEEP_MODE_FLAGS, &d8, &size);
	if (rv)
		return rv;
	info->sleep_supported = d8 & BIT(0) ? 1 : 0;

	return EC_SUCCESS;
}

test_export_static void batt_cbi_read_fet_info(void)
{
	struct fet_info *info = &default_battery_info.fuel_gauge.fet;
	uint8_t d8;
	uint8_t size;

	size = sizeof(info->reg_addr);
	cbi_get_board_info(CBI_TAG_BATT_FET_REG_ADDR, &info->reg_addr, &size);
	size = sizeof(info->reg_mask);
	cbi_get_board_info(CBI_TAG_BATT_FET_REG_MASK,
			   (uint8_t *)&info->reg_mask, &size);
	size = sizeof(info->disconnect_val);
	cbi_get_board_info(CBI_TAG_BATT_FET_DISCONNECT_VAL,
			   (uint8_t *)&info->disconnect_val, &size);
	size = sizeof(info->cfet_mask);
	cbi_get_board_info(CBI_TAG_BATT_FET_CFET_MASK,
			   (uint8_t *)&info->cfet_mask, &size);
	size = sizeof(info->cfet_off_val);
	cbi_get_board_info(CBI_TAG_BATT_FET_CFET_OFF_VAL,
			   (uint8_t *)&info->cfet_off_val, &size);
	size = sizeof(d8);
	if (cbi_get_board_info(CBI_TAG_BATT_FET_FLAGS, &d8, &size)
			== EC_SUCCESS)
		info->mfgacc_support = d8 & BIT(0) ? 1 : 0;
}

test_export_static void batt_cbi_read_fuel_gauge_info(void)
{
	struct fuel_gauge_info *info = &default_battery_info.fuel_gauge;
	uint8_t d8;
	uint8_t size;

	size = sizeof(info->manuf_name);
	cbi_get_board_info(CBI_TAG_FUEL_GAUGE_DEVICE_NAME,
			   (uint8_t *)&info->manuf_name, &size);
	size = sizeof(info->device_name);
	cbi_get_board_info(CBI_TAG_FUEL_GAUGE_DEVICE_NAME,
			   (uint8_t *)&info->device_name, &size);
	size = sizeof(d8);
	if (cbi_get_board_info(CBI_TAG_FUEL_GAUGE_FLAGS, &d8, &size)
			== EC_SUCCESS)
		info->override_nil = d8 & BIT(0) ? 1 : 0;

	batt_cbi_read_ship_mode();
	batt_cbi_read_sleep_mode();
	batt_cbi_read_fet_info();
}

test_export_static void batt_cbi_read_battery_info(void)
{
	struct battery_info *info = &default_battery_info.batt_info;
	uint8_t size;

	/*
	 * If each of these succeeds, 'info' will be updated. If it fails,
	 * default value will remain.
	 */
	size = sizeof(info->voltage_max);
	cbi_get_board_info(CBI_TAG_BATT_VOLTAGE_MAX,
			   (uint8_t *)&info->voltage_max, &size);
	size = sizeof(info->voltage_normal);
	cbi_get_board_info(CBI_TAG_BATT_VOLTAGE_NORMAL,
			   (uint8_t *)&info->voltage_normal, &size);
	size = sizeof(info->voltage_min);
	cbi_get_board_info(CBI_TAG_BATT_VOLTAGE_MIN,
			   (uint8_t *)&info->voltage_min, &size);
	size = sizeof(info->precharge_voltage);
	cbi_get_board_info(CBI_TAG_BATT_PRECHARGE_VOLTAGE,
			   (uint8_t *)&info->precharge_voltage, &size);
	size = sizeof(info->precharge_current);
	cbi_get_board_info(CBI_TAG_BATT_PRECHARGE_CURRENT,
			   (uint8_t *)&info->precharge_current, &size);
	size = sizeof(info->start_charging_min_c);
	cbi_get_board_info(CBI_TAG_BATT_START_CHARGING_MIN_C,
			   (uint8_t *)&info->start_charging_min_c, &size);
	size = sizeof(info->start_charging_max_c);
	cbi_get_board_info(CBI_TAG_BATT_START_CHARGING_MAX_C,
			   (uint8_t *)&info->start_charging_max_c, &size);
	size = sizeof(info->charging_min_c);
	cbi_get_board_info(CBI_TAG_BATT_CHARGING_MIN_C,
			   (uint8_t *)&info->charging_min_c,
			   &size);
	size = sizeof(info->charging_max_c);
	cbi_get_board_info(CBI_TAG_BATT_CHARGING_MAX_C,
			   (uint8_t *)&info->charging_max_c, &size);
	size = sizeof(info->discharging_min_c);
	cbi_get_board_info(CBI_TAG_BATT_DISCHARGING_MIN_C,
			   (uint8_t *)&info->discharging_min_c, &size);
	size = sizeof(info->discharging_max_c);
	cbi_get_board_info(CBI_TAG_BATT_DISCHARGING_MAX_C,
			   (uint8_t *)&info->discharging_max_c, &size);
}

test_export_static void batt_cbi_main(void)
{
	batt_cbi_read_fuel_gauge_info();
	batt_cbi_read_battery_info();
}
DECLARE_HOOK(HOOK_INIT, batt_cbi_main, HOOK_PRIO_DEFAULT);
