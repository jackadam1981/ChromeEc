/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Boot Mode detection and off-mode charging boot functions
 */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "charge_state.h"
#include "common.h"
#include "hooks.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)

#define BATTERY_CHECK_TIMEOUT_OFF_MODE_CHARGING K_SECONDS(10)

#define BATTERY_DISCHARGE_THRESHOLD 4

/* -1, if not yet cached or failed to cache */
static int cached_charge_percentage = -1;

static int shutdown_for_offmode_charging;

void board_chipset_pre_init(void)
{
	int value;
	if (!battery_state_of_charge_abs(&value)) {
		CPRINTS("Fetching battery charge state failed");
		return;
	}
	cached_charge_percentage = value;
	CPRINTS("Current battery charge = %d", cached_charge_percentage);

	/*
	 * TODO: move i2c from controller mode to peripheral mode and disable
	 * battery commands
	 */
	CPRINTS("I2C_PORT_ADSP switched to peripheral mode");
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, board_chipset_pre_init, HOOK_PRIO_DEFAULT);

void check_battery_discharge_expired(struct k_timer *timer_id)
{
	int value;
	if (!battery_state_of_charge_abs(&value)) {
		CPRINTS("Fetching battery charge state failed");
		return;
	}
	/*
	 * check if we excede the threshold for power on
	 */
	if (cached_charge_percentage - value > BATTERY_DISCHARGE_THRESHOLD) {
		/* TODO: Request power on */
	}
}
K_TIMER_DEFINE(check_battery_discharge, check_battery_discharge_expired, NULL);

void board_chipset_shutdown_complete(void)
{
	int value;
	/*
	 * TODO: move i2c from peripheral mode to controller mode and enable
	 * battery commands
	 */
	CPRINTS("I2C_PORT_ADSP switched to controller mode");

	/*
	 * if the reason for shutdown is due to off-mode charging
	 */
	if (shutdown_for_offmode_charging) {
		if (!battery_state_of_charge_abs(&value)) {
			CPRINTS("Fetching battery charge state failed");
			return;
		}
		/*
		 * Cache the battery percentage while we are in off mode
		 * charging state and start a timer to keep checking battery
		 * state every BATTERY_CHECK_TIMEOUT_OFF_MODE_CHARGING seconds
		 */
		cached_charge_percentage = value;
		k_timer_start(&check_battery_discharge,
			      BATTERY_CHECK_TIMEOUT_OFF_MODE_CHARGING,
			      BATTERY_CHECK_TIMEOUT_OFF_MODE_CHARGING);
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN_COMPLETE, board_chipset_shutdown_complete,
	     HOOK_PRIO_DEFAULT);
