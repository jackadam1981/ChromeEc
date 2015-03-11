/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */
#include "battery.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"

static int force_pre_charge;

static const struct battery_info info = {
	.voltage_max    = 4350,		/* mV */
	.voltage_normal = 4300,
	.voltage_min    = 3328,
	.precharge_current  = 256,	/* mA */
	.start_charging_min_c = 0,
	.start_charging_max_c = 45,
	.charging_min_c       = 0,
	.charging_max_c       = 45,
	.discharging_min_c    = 0,
	.discharging_max_c    = 60,
};

const struct battery_info *battery_get_info(void)
{
	return &info;
}

void battery_override_params(struct batt_params *batt)
{
	int temp;

	if (!(batt->flags & BATT_FLAG_BAD_TEMPERATURE)) {
		temp = DECI_KELVIN_TO_CELSIUS(batt->temperature);

		if (temp < 0) {
			batt->desired_voltage = 4350;
			batt->desired_current = 0;
			batt->flags |= BATT_FLAG_BAD_ANY;
		} else if (temp < 12) {
			batt->desired_voltage = 4350;
			batt->desired_current = 1500;
			batt->flags |= BATT_FLAG_WANT_CHARGE;
		} else if (temp < 50) {
			batt->desired_voltage = 4350;
			batt->desired_current = 3500;
			batt->flags |= BATT_FLAG_WANT_CHARGE;
		} else if (temp < 55) {
			batt->desired_voltage = 4110;
			batt->desired_current = 3500;
			batt->flags |= BATT_FLAG_WANT_CHARGE;
		} else {
			batt->desired_voltage = 4110;
			batt->desired_current = 0;
			batt->flags |= BATT_FLAG_BAD_ANY;
		}

		if (force_pre_charge) {
			batt->desired_current = 256;
		}
	}
}

static int cutoff(void)
{
	gpio_set_level(GPIO_BAT_CUT_OFF, 0);
	return EC_SUCCESS;
}

int board_cut_off_battery(void)
{
	return cutoff();
}

static void minnie_second_task(void)
{
	static int pre_ac, ac, second;
	struct batt_params batt;

	ac = extpower_is_present();

	if (ac != pre_ac) {
		battery_get_params(&batt);

		if (ac && (batt.state_of_charge == 100)) {
			batt.state_of_charge = 99;
			force_pre_charge = 1;
			second = 5;
		}
		else {
			force_pre_charge = 0;
		}

		pre_ac = ac;
	}

	if (second) {
		--second;
	}
	else {
		force_pre_charge = 0;
	}
}
DECLARE_HOOK(HOOK_SECOND, minnie_second_task, HOOK_PRIO_DEFAULT);
