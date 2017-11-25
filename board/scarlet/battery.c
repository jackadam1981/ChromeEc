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

/*
 * AE-Tech battery pack has two charging phases when operating
 * between 10 and 20C
 */
#define CHARGE_PHASE_CHANGE_TRIP_VOLTAGE_MV 4200
#define CHARGE_PHASE_CHANGE_HYSTERESIS_MV 50
#define CHARGE_PHASE_CHANGED_CURRENT_MA 1800

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

	/*
	 * Keep track of battery temperature range:
	 *
	 *        ZONE_0   ZONE_1     ZONE_2
	 * -----+--------+--------+------------+----- Temperature (C)
	 *      t0       t1       t2           t3
	 */
	enum {
		TEMP_ZONE_0, /* t0 < bat_temp_c <= t1 */
		TEMP_ZONE_1, /* t1 < bat_temp_c <= t2 */
		TEMP_ZONE_2, /* t2 < bat_temp_c <= t3 */
		TEMP_OUT_OF_RANGE, /* bat_temp_c <= t0 or > t3 */
	} temp_zone;

	static const struct {
		int temp_min; /* 0.1 deg C */
		int temp_max; /* 0.1 deg C */
		int desired_current; /* mA */
		int desired_voltage; /* mV */
	} temp_zones[] = {
		{0, 100, 900, 4200}, /* TEMP_ZONE_0 */
		{100, 200, 2700, 4350}, /* TEMP_ZONE_1 */
		{200, 450, 3500, 4350}, /* TEMP_ZONE_2 */
	};

	static int charge_phase = 1;

	if ((curr->batt.flags & BATT_FLAG_BAD_TEMPERATURE) ||
	    (bat_temp_c < temp_zones[0].temp_min) ||
	    (bat_temp_c >= temp_zones[ARRAY_SIZE(temp_zones) - 1].temp_max))
		temp_zone = TEMP_OUT_OF_RANGE;
	else {
		temp_zone = 0;
		while (temp_zone < ARRAY_SIZE(temp_zones)) {
			if (bat_temp_c < temp_zones[temp_zone].temp_max)
				break;
			temp_zone++;
		}
	}

	if (curr->state != ST_CHARGE) {
		charge_phase = 1;
		return 0;
	}

	switch (temp_zone) {
	case TEMP_ZONE_0:
	case TEMP_ZONE_2:
		curr->requested_current =
			temp_zones[temp_zone].desired_current;
		curr->requested_voltage =
			temp_zones[temp_zone].desired_voltage;
		break;
	case TEMP_ZONE_1:
		/*
		 * If the voltage reading is bad, let's be conservative
		 * and assume change_phase == 1.
		 */
		if (curr->batt.flags & BATT_FLAG_BAD_VOLTAGE)
			charge_phase = 1;
		else {
			if (curr->batt.voltage <
			    (CHARGE_PHASE_CHANGE_TRIP_VOLTAGE_MV -
			     CHARGE_PHASE_CHANGE_HYSTERESIS_MV))
				charge_phase = 0;
			else if (curr->batt.voltage >
				 CHARGE_PHASE_CHANGE_TRIP_VOLTAGE_MV)
				charge_phase = 1;
		}

		curr->requested_voltage =
			temp_zones[temp_zone].desired_voltage;

		curr->requested_current = (charge_phase) ?
			CHARGE_PHASE_CHANGED_CURRENT_MA :
			temp_zones[temp_zone].desired_current;
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
