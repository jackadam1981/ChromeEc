/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "battery_smart.h"
#include "charge_state.h"
#include "console.h"
#include "driver/charger/rt946x.h"
#include "ec_commands.h"
#include "extpower.h"
#include "util.h"

static const struct battery_info info = {
	.voltage_max		= 4350,
	.voltage_normal		= 3800,
	.voltage_min		= 3000,
	.precharge_current	= 700,
	.start_charging_min_c	= 0,
	.start_charging_max_c	= 45,
	.charging_min_c		= 0,
	.charging_max_c		= 45,
	.discharging_min_c	= -20,
	.discharging_max_c	= 55,
};

const struct battery_info *battery_get_info(void)
{
	return &info;
}

int board_cut_off_battery(void)
{
	return rt946x_cutoff_battery();
}

enum battery_disconnect_state battery_get_disconnect_state(void)
{
	if (battery_is_present() == BP_YES)
		return BATTERY_NOT_DISCONNECTED;
	return BATTERY_DISCONNECTED;
}

int charger_profile_override(struct charge_state_data *curr)
{
	/* battery temp in 0.1 deg C */
	int bat_temp_c = curr->batt.temperature - 2731;

	int bat_voltage;
	static int prev_bat_voltage;

	/*
	 * Keep track of battery temperature range:
	 *
	 *        ZONE_1   ZONE_2     ZONE_3
	 * -----+--------+--------+------------+----- Temperature (C)
	 *      0        10       20           45
	 *
	 * 0.2C of hysteresis is added during zone transition.
	 */
	static enum {
		TEMP_ZONE_1, /* 0 - 10C */
		TEMP_ZONE_2, /* 10 - 20C */
		TEMP_ZONE_3, /* 20 - 45C */
		TEMP_OUT_OF_RANGE /* < 0C or > 45C */
	} temp_range = TEMP_ZONE_3;

	if (!(curr->batt.flags & BATT_FLAG_BAD_TEMPERATURE)) {
		if (bat_temp_c < -1 || bat_temp_c > 451)
			temp_range = TEMP_OUT_OF_RANGE;
		else if (bat_temp_c > 1 && bat_temp_c < 99)
			temp_range = TEMP_ZONE_1;
		else if (bat_temp_c > 101 && bat_temp_c < 199)
			temp_range = TEMP_ZONE_2;
		else if (bat_temp_c > 201 && bat_temp_c < 449)
			temp_range = TEMP_ZONE_3;
	} else
		temp_range = TEMP_OUT_OF_RANGE;

	if (curr->state != ST_CHARGE)
		return 0;

	/* If the voltage reading is bad, fall back to the previous one. */
	if (curr->batt.flags & BATT_FLAG_BAD_VOLTAGE)
		bat_voltage = prev_bat_voltage;
	else
		bat_voltage = prev_bat_voltage = curr->batt.voltage;

	switch (temp_range) {
	case TEMP_ZONE_1:
		curr->requested_current = 900;
		curr->requested_voltage = 4200;
		break;
	case TEMP_ZONE_2:
		curr->requested_current = (bat_voltage < 4200) ? 2700 : 1800;
		break;
	case TEMP_ZONE_3:
		break;
	case TEMP_OUT_OF_RANGE:
		curr->requested_current = curr->requested_voltage = 0;
		curr->batt.flags &= ~BATT_FLAG_WANT_CHARGE;
		curr->state = ST_IDLE;
		break;
	}

	return 0;
}

/* Customs options controllable by host command. */
#define PARAM_FASTCHARGE (CS_PARAM_CUSTOM_PROFILE_MIN + 0)

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
