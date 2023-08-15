/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "charge_state.h"
#include "gpio.h"
#include "hooks.h"
#include "intelrvp.h"
#include "tcpm/tcpci.h"
int board_dc_jack_present(void)
{
#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
	return gpio_get_level(GPIO_DC_JACK_PRESENT);
#else
	return 0;
#endif /* CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0 */
}

static void board_dc_jack_handle(void)
{
#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
	struct charge_port_info charge_dc_jack;

	/* System is booted from DC Jack */
	if (board_dc_jack_present()) {
		charge_dc_jack.current =
			(PD_MAX_POWER_MW * 1000) / DC_JACK_MAX_VOLTAGE_MV;
		charge_dc_jack.voltage = DC_JACK_MAX_VOLTAGE_MV;
	} else {
		charge_dc_jack.current = 0;
		charge_dc_jack.voltage = USB_CHARGER_VOLTAGE_MV;
	}

	charge_manager_update_charge(CHARGE_SUPPLIER_DEDICATED,
				     DEDICATED_CHARGE_PORT, &charge_dc_jack);
#endif /* CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0 */
}

void board_dc_jack_interrupt(enum gpio_signal signal)
{
	board_dc_jack_handle();
}

static void board_charge_init(void)
{
	int port, supplier;
	struct charge_port_info charge_init = {
		.current = 0,
		.voltage = USB_CHARGER_VOLTAGE_MV,
	};

	/* Initialize all charge suppliers to seed the charge manager */
	for (port = 0; port < CHARGE_PORT_COUNT; port++) {
		for (supplier = 0; supplier < CHARGE_SUPPLIER_COUNT;
		     supplier++) {
			charge_manager_update_charge(supplier, port,
						     &charge_init);
		}
	}

	board_dc_jack_handle();
}
DECLARE_HOOK(HOOK_INIT, board_charge_init, HOOK_PRIO_DEFAULT);
