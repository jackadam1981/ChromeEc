/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "charge_state.h"
#include "console.h"
#include "util.h"

static const struct battery_info info = {
	/*
	 * Design voltage
	 *   max    = 8.4V
	 *   normal = 7.4V
	 *   min    = 6.0V
	 */
	.voltage_max    = 8400,
	.voltage_normal = 7400,
	.voltage_min    = 6000,

	/* Pre-charge current: I <= 0.01C */
	.precharge_current  = 64, /* mA */

	/*
	 * Operational temperature range
	 *   0 <= T_charge    <= 50 deg C
	 * -20 <= T_discharge <= 60 deg C
	 */
	.start_charging_min_c = 0,
	.start_charging_max_c = 50,
	.charging_min_c       = 0,
	.charging_max_c       = 50,
	.discharging_min_c    = -20,
	.discharging_max_c    = 60,
};

const struct battery_info *battery_get_info(void)
{
	return &info;
}

#ifdef CONFIG_CHARGER_PROFILE_OVERRIDE

static int fast_charging_allowed = 0;

/**
 * This can override the smart battery's charging profile. To make a change,
 * just modify one or more of curr->requested_voltage, curr->requested_current,
 * or curr->state. Leave everything else unchanged.
 */
void charger_profile_override(struct charge_state_data *curr)
{
	if (!fast_charging_allowed)
		return;

	/* We only want to override how we charge, nothing else. */
	if (curr->state != ST_CHARGE)
		return;

	/* Okay, impose our custom will */
	curr->requested_current = 9000;
	curr->requested_voltage = 8300;
	if (curr->batt.current <= 6300) {
		curr->requested_current = 6300;
		curr->requested_voltage = 8400;
	} else if (curr->batt.current <= 4500) {
		curr->requested_current = 4500;
		curr->requested_voltage = 8500;
	} else if (curr->batt.current <= 2700) {
		curr->requested_current = 2700;
		curr->requested_voltage = 8700;
	} else if (curr->batt.current <= 475) {
		/*
		 * HEY: Should we stop? If so, how do we start again?
		 * For now, just use the battery's profile.
		 */
		curr->requested_current = curr->batt.desired_current;
		curr->requested_voltage = curr->batt.desired_voltage;
	}
}

int charger_profile_override_enable(int enable)
{
	fast_charging_allowed = enable;
	return EC_SUCCESS;
}

static int command_fastcharge(int argc, char **argv)
{
        if (argc > 1 && !parse_bool(argv[1], &fast_charging_allowed))
		return EC_ERROR_PARAM1;

	ccprintf("fastcharge %s\n", fast_charging_allowed ? "on" : "off");

        return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(fastcharge, command_fastcharge,
			"[on|off]",
			"Get or set fast charging profile",
			NULL);

#endif	/* CONFIG_CHARGER_PROFILE_OVERRIDE */
