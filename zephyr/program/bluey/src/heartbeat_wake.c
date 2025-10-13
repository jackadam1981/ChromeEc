/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Bluey heartbeat wake implementation. */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "extpower.h"
#include "hooks.h"
#include "system.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)

/* Timeout to check for the remaining battery charge during off-mode charging */
#define BATTERY_SoC_CHECK_TIMEOUT K_SECONDS(10)

#define BATTERY_STATE_OF_CHARGE_LOWER_THRESHOLD 98

int get_battery_state_of_charge(void)
{
	struct batt_params _batt;
	const struct batt_params *batt = &_batt;
	battery_get_params(&_batt);

	return batt->state_of_charge;
}

void check_battery_discharge_expired(struct k_work *work)
{
	int battery_soc;

	battery_soc = get_battery_state_of_charge();

	if (battery_soc == -1) {
		CPRINTS("Invalid charge battery_soc");
		return;
	}

	CPRINTS("heartbeat wake: Current battery State of Charge = %d",
		battery_soc);
	/*
	 * check if current battery is below the Sustain lower threshold
	 */
	if (battery_soc < BATTERY_STATE_OF_CHARGE_LOWER_THRESHOLD) {
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

/* Stop the timer before turning the AP back on */
void board_chipset_pre_init_heartbeat_stop(void)
{
	k_timer_stop(&check_battery_discharge);
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, board_chipset_pre_init_heartbeat_stop,
	     HOOK_PRIO_DEFAULT);

/* once shutdown is completed check if the AC is connected
 * if yes, start the timer to check for battery discharge
 * every BATTERY_SoC_CHECK_TIMEOUT
 */
void board_chipset_shutdown_complete_heartbeat_start(void)
{
	if (extpower_is_present() && battery_is_present() == BP_YES)
		k_timer_start(&check_battery_discharge,
			      BATTERY_SoC_CHECK_TIMEOUT,
			      BATTERY_SoC_CHECK_TIMEOUT);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN_COMPLETE,
	     board_chipset_shutdown_complete_heartbeat_start,
	     HOOK_PRIO_DEFAULT);
