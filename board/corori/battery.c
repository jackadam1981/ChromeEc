/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */
#include "battery.h"
#include "battery_fuel_gauge.h"
#include "battery_smart.h"
#include "charge_state.h"
#include "common.h"
#include "gpio.h"
#include "temp_sensor.h"
#include "thermal.h"
#include "util.h"

/*
 * Battery info for lalala battery types. Note that the fields
 * start_charging_min/max and charging_min/max are not used for the charger.
 * The effective temperature limits are given by discharging_min/max_c.
 *
 * Fuel Gauge (FG) parameters which are used for determining if the battery
 * is connected, the appropriate ship mode (battery cutoff) command, and the
 * charge/discharge FETs status.
 *
 * Ship mode (battery cutoff) requires 2 writes to the appropriate smart battery
 * register. For some batteries, the charge/discharge FET bits are set when
 * charging/discharging is active, in other types, these bits set mean that
 * charging/discharging is disabled. Therefore, in addition to the mask for
 * these bits, a disconnect value must be specified. Note that for TI fuel
 * gauge, the charge/discharge FET status is found in Operation Status (0x54),
 * but a read of Manufacturer Access (0x00) will return the lower 16 bits of
 * Operation status which contains the FET status bits.
 *
 * The assumption for battery types supported is that the charge/discharge FET
 * status can be read with a sb_read() command and therefore, only the register
 * address, mask, and disconnect value need to be provided.
 */
const struct board_batt_params board_battery_info[] = {
		/* C21N2018 Battery Information
		 * TODO(b:196506846):Need to be verified on the Proto
		 */
	 [BATTERY_C21N2018] = {
		.fuel_gauge = {
			.manuf_name = "AS3GXXD3KA",
			.device_name = "C110160",
			.ship_mode = {
				.reg_addr = 0x00,
				.reg_data = { 0x0010, 0x0010 },
			},
			.fet = {
				.reg_addr = 0x0,
				.reg_mask = 0x2000,
				.disconnect_val = 0x2000,
				.cfet_mask = 0x4000,
				.cfet_off_val = 0x4000,
			},
		},
		.batt_info = {
			.voltage_max            = 8800,
			.voltage_normal         = 7890,
			.voltage_min            = 6000,
			.precharge_current      = 256,
			.start_charging_min_c   = 0,
			.start_charging_max_c   = 45,
			.charging_min_c         = 0,
			.charging_max_c         = 60,
			.discharging_min_c      = -20,
			.discharging_max_c      = 60,
		},
	},
};
BUILD_ASSERT(ARRAY_SIZE(board_battery_info) == BATTERY_TYPE_COUNT);

const enum battery_type DEFAULT_BATTERY_TYPE = BATTERY_C21N2018;

struct chg_curr_step {
	int on;
	int off;
	int curr_ma;
};

static const struct chg_curr_step chg_curr_table[] = {
	{.on = 0, .off = 45, .curr_ma = 2800},
	{.on = 55, .off = 45, .curr_ma = 1500},
	{.on = 60, .off = 50, .curr_ma = 1000},
};

/* All charge current tables must have the same number of levels */
#define NUM_CHG_CURRENT_LEVELS ARRAY_SIZE(chg_curr_table)

int charger_profile_override(struct charge_state_data *curr)
{
	int rv;
	int chg_temp_c;
	int current;
	int thermal_sensor0;
	static int current_level;
	static int prev_tmp;

	/*
	 * Precharge must be executed when communication is failed on
	 * dead battery.
	 */
	if (!(curr->batt.flags & BATT_FLAG_RESPONSIVE))
		return 0;

	current = curr->requested_current;

	rv = temp_sensor_read(TEMP_SENSOR_2, &thermal_sensor0);
	chg_temp_c = K_TO_C(thermal_sensor0);

	if (rv != EC_SUCCESS)
		return 0;

	if (chipset_in_state(CHIPSET_STATE_ON)) {
		if (chg_temp_c < prev_tmp) {
			if (chg_temp_c <= chg_curr_table[current_level].off)
				current_level = current_level - 1;
		} else if (chg_temp_c > prev_tmp) {
			if (chg_temp_c >= chg_curr_table[current_level + 1].on)
				current_level = current_level + 1;
		}
		/*
		 * Prevent level always minus 0 or over table steps.
		 */
		if (current_level < 0)
			current_level = 0;
		else if (current_level >= NUM_CHG_CURRENT_LEVELS)
			current_level = NUM_CHG_CURRENT_LEVELS - 1;

		prev_tmp = chg_temp_c;
		current = chg_curr_table[current_level].curr_ma;

		curr->requested_current = MIN(curr->requested_current, current);
	}
	return 0;
}

enum ec_status charger_profile_override_get_param(uint32_t param,
						  uint32_t *value)
{
	return EC_RES_INVALID_PARAM;
}

enum ec_status charger_profile_override_set_param(uint32_t param,
						  uint32_t value)
{
	return EC_RES_INVALID_PARAM;
}
