/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "battery_smart.h"
#include "charge_state.h"
#include "console.h"
#include "driver/charger/isl923x.h"
#include "ec_commands.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "usb_pd.h"
#include "util.h"

/* Shutdown mode parameter to write to manufacturer access register */
#define SB_SHIP_MODE_REG	SB_MANUFACTURER_ACCESS
#define SB_SHUTDOWN_DATA	0x0010

#define TEMP_OUT_OF_RANGE TEMP_ZONE_COUNT

#define BATT_ID 0

#define BAT_LEVEL_PD_LIMIT 85
#define IBAT_PD_LIMIT 1000

#define BATTERY_SIMPLO_CHARGE_MIN_TEMP 0
#define BATTERY_SIMPLO_CHARGE_MAX_TEMP 60

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

enum battery_type {
	BATTERY_SIMPLO = 0,
	BATTERY_COUNT
};

static const struct battery_info info[] = {
	[BATTERY_SIMPLO] = {
		.voltage_max		= 4400 * 3,
		.voltage_normal		= 3860 * 3,
		.voltage_min		= 3000 * 3,
		.precharge_current	= 256,
		.start_charging_min_c	= 0,
		.start_charging_max_c	= 45,
		.charging_min_c		= 0,
		.charging_max_c		= 60,
		.discharging_min_c	= -20,
		.discharging_max_c	= 60,
	},
};

const struct battery_info *battery_get_info(void)
{
	return &info[BATT_ID];
}

int board_cut_off_battery(void)
{
	int rv;

	/* Ship mode command must be sent twice to take effect */
	rv = sb_write(SB_SHIP_MODE_REG, SB_SHUTDOWN_DATA);

	if (rv != EC_SUCCESS)
		return rv;

	return sb_write(SB_SHIP_MODE_REG, SB_SHUTDOWN_DATA);
}

enum battery_disconnect_state battery_get_disconnect_state(void)
{
	if (battery_is_present() == BP_YES)
		return BATTERY_NOT_DISCONNECTED;
	return BATTERY_DISCONNECTED;
}

int charger_profile_override(struct charge_state_data *curr)
{
	const struct battery_info *batt_info;
	/* battery temp in 0.1 deg C */
	int bat_temp_c = curr->batt.temperature - 2731;

	batt_info = battery_get_info();
	/* Don't charge if outside of allowable temperature range */
	if (bat_temp_c >= batt_info->charging_max_c * 10 ||
	    bat_temp_c < batt_info->charging_min_c * 10) {
		curr->requested_current = 0;
		curr->requested_voltage = 0;
		curr->batt.flags &= ~BATT_FLAG_WANT_CHARGE;
		curr->state = ST_IDLE;
	}
	return 0;
}

static void board_charge_termination(void)
{
	static uint8_t te;
	/* Enable charge termination when we are sure battery is present. */
	if (!te && battery_is_present() == BP_YES) {
		/*
		 * TODO: implement this for isl9238
		if (!rt946x_enable_charge_termination(1))
			te = 1;
		*/
	}
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE,
	     board_charge_termination,
	     HOOK_PRIO_DEFAULT);

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
