/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common USB PD charge configuration */

#include "charge_manager.h"
#include "charge_state.h"
#include "gpio.h"
#include "hooks.h"
#include "intelrvp.h"
#include "tcpm/tcpci.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)

bool is_typec_port(int port)
{
#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
	return !(port == DEDICATED_CHARGE_PORT || port == CHARGE_PORT_NONE);
#else
	return !(port == CHARGE_PORT_NONE);
#endif /* CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0 */
}

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

#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
	/*
	 * Do not enable Type-C port if the DC Jack is present.
	 * When the Type-C is active port, hardware circuit will
	 * block DC jack from enabling +VADP_OUT.
	 */
	if (port != DEDICATED_CHARGE_PORT && board_dc_jack_present()) {
		CPRINTS("DC Jack present, Skip enable p%d", port);
		return EC_ERROR_INVAL;
	}
#endif /* CONFIG_DEDICATED_CHARGE_PORT_COUNT */

	/* Make sure non-charging ports are disabled */
	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		if (i != port) {
			board_charging_enable(i, 0);
		}
	}

	/* Enable charging port */
	if (is_typec_port(port)) {
		board_charging_enable(port, 1);
	}

	CPRINTS("New chg p%d", port);

	return EC_SUCCESS;
}
