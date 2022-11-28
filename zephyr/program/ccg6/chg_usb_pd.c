/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common USB PD charge configuration */

#include "charge_manager.h"
#include "charge_state_v2.h"
#include "ccg6.h"
#include "gpio.h"
#include "hooks.h"
#include "tcpm/tcpci.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)

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
}
DECLARE_HOOK(HOOK_INIT, board_charge_init, HOOK_PRIO_DEFAULT);

int board_set_active_charge_port(int port)
{
	int i;
	/* charge port is a realy physical port */
	int is_real_port = (port >= 0 && port < CHARGE_PORT_COUNT);
	/* check if we are source vbus on that port */
	int source = board_vbus_source_enabled(port);

	if (is_real_port && source) {
		CPRINTS("Skip enable p%d", port);
		return EC_ERROR_INVAL;
	}

	/* Make sure non-charging ports are disabled */
	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		if (i != port) {
			board_charging_enable(i, 0);
		}
	}

	/* Enable charging port */
	if (port != CHARGE_PORT_NONE) {
		board_charging_enable(port, 1);
	}

	CPRINTS("New chg p%d", port);

	return EC_SUCCESS;
}
