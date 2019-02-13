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
#include "driver/battery/mm8013.h"
#include "driver/charger/rt946x.h"
#include "driver/tcpm/mt6370.h"
#include "ec_commands.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "usb_pd.h"
#include "util.h"

#define TEMP_OUT_OF_RANGE TEMP_ZONE_COUNT

/* We have only one battery now. */
#define BATT_ID 0

#define BAT_LEVEL_PD_LIMIT 85

#define BATTERY_SIMPLO_CHARGE_MIN_TEMP 0
#define BATTERY_SIMPLO_CHARGE_MAX_TEMP 60

enum battery_type {
	BATTERY_MITSUMI = 0,
	BATTERY_COUNT
};

static const struct battery_info info = {
	.voltage_max		= 5500,
	.voltage_normal		= 3860,
	.voltage_min		= 2500,
	.precharge_current	= 200,
	.start_charging_min_c	= -20,
	.start_charging_max_c	= 85,
	.charging_min_c		= -20,
	.charging_max_c		= 85,
	.discharging_min_c	= -20,
	.discharging_max_c	= 85,
};

const struct battery_info *battery_get_info(void)
{
	return &info;
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
	/* TODO(phoenixshen): check battery temp here. */

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

static void pd_limit_5v(uint8_t en)
{
	int wanted_pd_voltage;

	wanted_pd_voltage = en ? 5500 : PD_MAX_VOLTAGE_MV;

	if (pd_get_max_voltage() != wanted_pd_voltage)
		pd_set_external_voltage_limit(0, wanted_pd_voltage);
}

/* When battery level > BAT_LEVEL_PD_LIMIT, we limit PD voltage to 5V. */
static void board_pd_voltage(void)
{
	pd_limit_5v(charge_get_percent() > BAT_LEVEL_PD_LIMIT);
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, board_pd_voltage, HOOK_PRIO_DEFAULT);

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
