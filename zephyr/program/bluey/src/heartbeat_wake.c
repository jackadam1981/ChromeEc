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
#include "power/qcom.h"
#include "system.h"
#include "util.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bluey_heartbeat_wake, LOG_LEVEL_INF);

/* Timeout to check for the remaining battery charge during off-mode charging */
#define BATTERY_SoC_CHECK_TIMEOUT K_SECONDS(10)

/*
 * TODO(b/439824946): Update this threshold when ADSP lite is available.
 *
 * The current 75% value is a temporary workaround. The absence of
 * ADSP lite firmware limits the system to slow charging, preventing
 * the battery from reaching 100%.
 */
#define BATTERY_STATE_OF_CHARGE_LOWER_THRESHOLD 75

#define BATTERY_BAD_STATE_OF_CHARGE -1

int get_battery_state_of_charge(void)
{
	struct batt_params batt;
	battery_get_params(&batt);

	if (batt.flags & BATT_FLAG_BAD_STATE_OF_CHARGE) {
		return BATTERY_BAD_STATE_OF_CHARGE;
	}

	return batt.state_of_charge;
}

void check_battery_discharge_expired(struct k_work *work)
{
	int battery_soc;

	battery_soc = get_battery_state_of_charge();

	if (BATTERY_BAD_STATE_OF_CHARGE == battery_soc) {
		LOG_ERR("Battery State of Charge invalid");
		return;
	}

	LOG_INF("Heartbeat wake: Current battery State of Charge = %d",
		battery_soc);

	/* Check if current battery is below the desired lower threshold. */
	if (battery_soc < BATTERY_STATE_OF_CHARGE_LOWER_THRESHOLD) {
		power_on_req_heartbeat();
		return;
	}
}
K_WORK_DEFINE(check_battery_discharge_work, check_battery_discharge_expired);

void check_battery_discharge_handler(struct k_timer *timer_id)
{
	/*
	 * Since we can't read the battery percentage in interrupt context
	 * we submit some work.
	 */
	k_work_submit(&check_battery_discharge_work);
}
K_TIMER_DEFINE(check_battery_discharge, check_battery_discharge_handler, NULL);

/* Stop the timer if AC disconnected. */
void extpower_changed_heartbeat_wake(void)
{
	if (!extpower_is_present()) {
		LOG_INF("AC Disconnected: heartbeat wake stopped");
		k_timer_stop(&check_battery_discharge);
	}
}
DECLARE_HOOK(HOOK_AC_CHANGE, extpower_changed_heartbeat_wake, HOOK_PRIO_FIRST);

/* Stop the timer before turning the AP back on. */
void board_chipset_pre_init_heartbeat_stop(void)
{
	k_timer_stop(&check_battery_discharge);
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, board_chipset_pre_init_heartbeat_stop,
	     HOOK_PRIO_DEFAULT);

/*
 * Once shutdown is completed check if the AC is connected
 * if yes, start the timer to check for battery discharge
 * every BATTERY_SoC_CHECK_TIMEOUT.
 */
void board_chipset_shutdown_complete_heartbeat_start(void)
{
	if (extpower_is_present() && battery_is_present() == BP_YES) {
		LOG_INF("AC detected, heartbeat wake enabled");
		/*
		 * Submit work to check and immediately start
		 * the charging.
		 */
		k_work_submit(&check_battery_discharge_work);
		k_timer_start(&check_battery_discharge,
			      BATTERY_SoC_CHECK_TIMEOUT,
			      BATTERY_SoC_CHECK_TIMEOUT);
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN_COMPLETE,
	     board_chipset_shutdown_complete_heartbeat_start,
	     HOOK_PRIO_DEFAULT);
