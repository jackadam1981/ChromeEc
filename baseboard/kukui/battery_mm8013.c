/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "charge_state.h"
#include "console.h"
#include "driver/battery/mm8013.h"
#include "driver/charger/rt946x.h"
#include "driver/tcpm/mt6370.h"
#include "ec_commands.h"
#include "hooks.h"
#include "power.h"
#include "usb_pd.h"
#include "util.h"

#define TEMP_OUT_OF_RANGE TEMP_ZONE_COUNT

#define BATT_ID 0

#define BAT_LEVEL_PD_LIMIT 85
#define IBAT_PD_LIMIT 1000

#define BATTERY_SIMPLO_CHARGE_MIN_TEMP 0
#define BATTERY_SIMPLO_CHARGE_MAX_TEMP 60

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

enum battery_type {
	BATTERY_SCUD = 0,
	BATTERY_COUNT
};

static const struct battery_info info[] = {
	[BATTERY_SCUD] = {
		.voltage_max		= 4400,
		.voltage_normal		= 3850,
		.voltage_min		= 3400,
		.precharge_current	= 256,
		.start_charging_min_c	= 0,
		.start_charging_max_c	= 45,
		.charging_min_c		= 0,
		.charging_max_c		= 50,
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
	/* The cut-off procedure is recommended by Richtek. b/116682788 */
	rt946x_por_reset();
	mt6370_vconn_discharge(0);
	rt946x_cutoff_battery();

	return EC_SUCCESS;
}

enum battery_disconnect_state battery_get_disconnect_state(void)
{
	if (battery_is_present() == BP_YES)
		return BATTERY_NOT_DISCONNECTED;
	return BATTERY_DISCONNECTED;
}

int charger_profile_override(struct charge_state_data *curr)
{
	static int previous_chg_limit_mv;
	int chg_limit_mv;

	/* TODO(b:131284131): Add battery configs for krane. */

	/* Limit input (=VBUS) to 5V when soc > 85% and charge current < 1A. */
	if (!(curr->batt.flags & BATT_FLAG_BAD_CURRENT) &&
			charge_get_percent() > BAT_LEVEL_PD_LIMIT &&
			curr->batt.current < 1000) {
		chg_limit_mv = 5500;
	} else if (IS_ENABLED(BOARD_KRANE) &&
			board_get_version() == 3 &&
			power_get_state() == POWER_S0) {
		/*
		 * TODO(b:134227872): limit power to 5V/2A in S0 to prevent
		 * overheat
		 */
		chg_limit_mv = 5500;
		curr->requested_current = 2000;
	} else {
		chg_limit_mv = PD_MAX_VOLTAGE_MV;
	}

	if (chg_limit_mv != previous_chg_limit_mv)
		CPRINTS("VBUS limited to %dmV", chg_limit_mv);
	previous_chg_limit_mv = chg_limit_mv;

	/* Pull down VBUS */
	if (pd_get_max_voltage() != chg_limit_mv)
		pd_set_external_voltage_limit(0, chg_limit_mv);

	/*
	 * When the charger says it's done charging, even if fuel gauge says
	 * SOC < BATTERY_LEVEL_NEAR_FULL, we'll overwrite SOC with
	 * BATTERY_LEVEL_NEAR_FULL. So we can ensure both Chrome OS UI
	 * and battery LED indicate full charge.
	 */
	if (rt946x_is_charge_done()) {
		curr->batt.state_of_charge = MAX(BATTERY_LEVEL_NEAR_FULL,
						 curr->batt.state_of_charge);
	}

	return 0;
}

static void board_charge_termination(void)
{
	static uint8_t te;
	/* Enable charge termination when we are sure battery is present. */
	if (!te && battery_is_present() == BP_YES) {
		if (!rt946x_enable_charge_termination(1))
			te = 1;
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
