/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Boot Mode detection and off-mode charging boot functions
 */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "system.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)

/* Timeout to check for the remaining battery charge during off-mode charging */
#define BATTERY_CHECK_TIMEOUT_OFF_MODE_CHARGING K_SECONDS(10)

/* Allowed discharge battery charge percentage before we start charging again */
#define BATTERY_DISCHARGE_THRESHOLD 4

/* -1, if not yet cached*/
static int cached_charge_percentage = -1;

/* Indicator to show if the shutdown was due to off-mode charging */
static int shutdown_for_offmode_charging;

int fetch_battery_charge(void)
{
	struct batt_params _batt;
	const struct batt_params *batt = &_batt;
	battery_get_params(&_batt);

	return batt->state_of_charge;
}

void check_battery_discharge_expired(struct k_work *work)
{
	int value;

	value = fetch_battery_charge();

	if (value == -1) {
		CPRINTS("Invalid charge value");
		return;
	}

	CPRINTS("Offmode-charging current battery charge = %d", value);
	/*
	 * check if we excede the threshold for power on
	 */
	if (cached_charge_percentage - value > BATTERY_DISCHARGE_THRESHOLD) {
		chipset_power_on();
	}
}
K_WORK_DEFINE(check_battery_discharge_work, check_battery_discharge_expired);

void check_battery_discharge_handler(struct k_timer *timer_id)
{
	/* Since we can't read the battery percentage in interrupt context
	 * we submit some work
	 */
	k_work_submit(&check_battery_discharge_work);
}
K_TIMER_DEFINE(check_battery_discharge, check_battery_discharge_handler, NULL);

void board_chipset_pre_init(void)
{
	int value;
	value = fetch_battery_charge();
	cached_charge_percentage = value;
	CPRINTS("Current battery charge = %d", cached_charge_percentage);
	/*
	 * TODO: move i2c from controller mode to peripheral mode and disable
	 * battery commands
	 */
	CPRINTS("I2C_PORT_ADSP switched to peripheral mode");
	/*
	 * Clean-up for offmode charging if enabled.
	 */
	k_timer_stop(&check_battery_discharge);
	shutdown_for_offmode_charging = 0;
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, board_chipset_pre_init, HOOK_PRIO_DEFAULT);

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
		value = fetch_battery_charge();
		/*
		 * Cache the battery percentage while we are in off mode
		 * charging state and start a timer to keep checking battery
		 * state every BATTERY_CHECK_TIMEOUT_OFF_MODE_CHARGING seconds
		 */
		cached_charge_percentage = value;

		CPRINTS("Offmode-charging, remaining battery charge = %d",
			cached_charge_percentage);
		k_timer_start(&check_battery_discharge,
			      BATTERY_CHECK_TIMEOUT_OFF_MODE_CHARGING,
			      BATTERY_CHECK_TIMEOUT_OFF_MODE_CHARGING);
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN_COMPLETE, board_chipset_shutdown_complete,
	     HOOK_PRIO_DEFAULT);

static int command_apshutdown_offmode(int argc, const char **argv)
{
	if (IS_ENABLED(CONFIG_POWER_BUTTON_INIT_IDLE)) {
		chip_save_reset_flags(chip_read_reset_flags() |
				      EC_RESET_FLAG_AP_IDLE);
		system_set_reset_flags(EC_RESET_FLAG_AP_IDLE);
		CPRINTS("off mode charging");
	}

	shutdown_for_offmode_charging = 1;
	chipset_force_shutdown(CHIPSET_SHUTDOWN_CONSOLE_CMD);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(apshutdown_offmode, command_apshutdown_offmode, NULL,
			"Force AP shutdown for offmode charging");
