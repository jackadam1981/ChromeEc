/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "charger.h"
#include "charger/isl923x_public.h"
#include "console.h"
#include "extpower.h"
#include "hooks.h"
#include "usb_mux.h"
#include "usb_pd.h"

#include <zephyr/logging/log.h>

#include <cros_board_info.h>

LOG_MODULE_REGISTER(charger, LOG_LEVEL_INF);

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

__override int board_get_leave_safe_mode_delay_ms(void)
{
	const struct batt_conf_embed *const batt = get_batt_conf();

	/* If it's COSMX battery, there's need more delay time. */
	if (!strcasecmp(batt->manuf_name, "COSMX KT0030B002") ||
	    !strcasecmp(batt->manuf_name, "COSMX KT0030B004"))
		return 2000;
	else
		return 500;
}

#define CHECK_BATT_STAT_DELAY_MS (500 * MSEC)
static int check_batt_retry;

void board_check_battery_status(void)
{
	enum battery_disconnect_state battery_disconnect_status =
		battery_get_disconnect_state();

	/*
	 * The following 2 states can read DFET status successfully
	 * so not need to do initialize battery type again.
	 * BATTERY_DISCONNECTED: The DFET is off.
	 * BATTERY_NOT_DISCONNECTED: The battery can discharge.
	 * Therefore, do initilialize only at BATTERY_DISCONNECT_ERROR.
	 */
	if (battery_disconnect_status != BATTERY_DISCONNECT_ERROR) {
		check_batt_retry = 0;
		return;
	}

	check_batt_retry++;
	if (check_batt_retry > 5) {
		LOG_INF("Board has retried init_battery_type 5 times.");
		check_batt_retry = 0;
		return;
	}

	LOG_INF("Retry init_battery_type: %d", check_batt_retry);
	init_battery_type();
}
DECLARE_DEFERRED(board_check_battery_status);

__override int board_get_default_battery_type(void)
{
	const struct batt_params *batt = charger_current_battery_params();

	if (batt->flags & BATT_FLAG_RESPONSIVE) {
		/* Check Battery status again after 500msec. */
		hook_call_deferred(&board_check_battery_status_data,
				   CHECK_BATT_STAT_DELAY_MS);
	} else {
		check_batt_retry = 0;
	}

	return DEFAULT_BATTERY_TYPE;
}

test_export_static void update_charger_config(void)
{
	uint32_t board_version;
	int ret;

	ret = cbi_get_board_version(&board_version);
	if (ret != 0)
		return;

	/* skip craaskana and craaswell */
	if (board_version == 0x0b || board_version == 0x0D)
		return;

	charger_set_frequency(1050);
}
DECLARE_HOOK(HOOK_INIT, update_charger_config, HOOK_PRIO_DEFAULT);
