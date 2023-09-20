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
#include "cros_board_info.h"
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

__override int board_get_leave_safe_mode_delay_ms(void)
{
	const struct fuel_gauge_info *const fuel_gauge =
		&get_batt_params()->fuel_gauge;

	/* If it's COSMX battery, there's need more delay time. */
	if (!strcasecmp(fuel_gauge->manuf_name, "COSMX KT0030B002") ||
	    !strcasecmp(fuel_gauge->manuf_name, "COSMX KT0030B004"))
		return 2000;
	else
		return 500;
}

static unsigned int board_pd_max_voltage;

__override unsigned int board_set_pd_max_voltage(void)
{
	return board_pd_max_voltage;
}

test_export_static void board_pd_max_voltage_init(void)
{
	uint32_t val, proj;

	board_pd_max_voltage = CONFIG_PLATFORM_EC_PD_MAX_VOLTAGE_MV;

	if (cbi_get_sku_id(&val) != EC_SUCCESS) {
		LOG_ERR("Error retrieving CBI SKU_ID.");
		return;
	}

	proj = val & 0x7fff0000;

	/*
	 * Project Craask, Craaskbowl and Craaskvin only support 45W
	 * as PD max power.
	 */
	if (proj == 0x40000 || proj == 0x50000 || proj == 0x60000) {
		board_pd_max_voltage = 15000;
		LOG_INF("Setting PD_MAX_VOLTAGE to 15V for 3 project.");
	}

	pd_set_max_voltage(board_pd_max_voltage);
}
DECLARE_HOOK(HOOK_INIT, board_pd_max_voltage_init, HOOK_PRIO_POST_I2C);
