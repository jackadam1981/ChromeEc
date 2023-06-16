/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "charge_manager.h"
#include "charger.h"
#include "charger/isl923x_public.h"
#include "console.h"
#include "extpower.h"
#include "hooks.h"
#include "usb_mux.h"
#include "usb_pd.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

int extpower_is_present(void)
{
	int port;
	int rv;
	bool acok;

	for (port = 0; port < board_get_usb_pd_port_count(); port++) {
		rv = raa489000_is_acok(port, &acok);
		if ((rv == EC_SUCCESS) && acok)
			return 1;
	}

	return 0;
}

/*
 * Craask does not have a GPIO indicating whether extpower is present,
 * so detect using the charger(s).
 */
__override void board_check_extpower(void)
{
	static int last_extpower_present;
	int extpower_present = extpower_is_present();

	if (last_extpower_present ^ extpower_present)
		extpower_handle_update(extpower_present);

	last_extpower_present = extpower_present;
}

__override void board_hibernate(void)
{
	/* Shut down the chargers */
	if (board_get_usb_pd_port_count() == 2)
		raa489000_hibernate(CHARGER_SECONDARY, true);
	raa489000_hibernate(CHARGER_PRIMARY, true);
	LOG_INF("Charger(s) hibernated");
	cflush();
}

static bool board_batt_stable;

static void board_batt_stable_deferred(void)
{
	board_batt_stable = true;
	LOG_INF("batt is stable.");
}
DECLARE_DEFERRED(board_batt_stable_deferred);

static void batt_stable_count_init(void)
{
	/*
	 * It needs 2s for cosmx batt get stable after
	 * gpio_ec_battery_pres_odl is low
	 */
	if (battery_is_present() == BP_YES) {
		LOG_INF("wait batt statble count 2s");
		hook_call_deferred(&board_batt_stable_deferred_data,
				   (2 * SECOND));
	}
}
DECLARE_HOOK(HOOK_INIT, batt_stable_count_init, HOOK_PRIO_INIT_CHARGE_MANAGER);

__override bool board_can_leave_safe_mode(void)
{
	const struct fuel_gauge_info *const fuel_gauge =
		&get_batt_params()->fuel_gauge;

	/* If it's not COSMX battery, there's no need more delay time. */
	if (strcasecmp(fuel_gauge->manuf_name, "COSMX KT0030B002") &&
	    strcasecmp(fuel_gauge->manuf_name, "COSMX KT0030B004"))
		return true;

	/* cosmx battery might need about 2s to get stable. */
	if (board_batt_stable)
		return true;

	return false;
}
