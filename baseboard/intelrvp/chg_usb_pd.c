/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common USB PD charge configuration */

#include "charge_manager.h"
#include "charge_state_v2.h"
#include "hooks.h"
#include "tcpm/tcpci.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

bool is_typec_port(int port)
{
#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
	return !(port == board_get_dedicated_charge_port() ||
			port == CHARGE_PORT_NONE);
#else
	return !(port == CHARGE_PORT_NONE);
#endif /* CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0 */
}

static inline int board_dc_jack_present(void)
{
#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
	return gpio_get_level(GPIO_DC_JACK_PRESENT);
#else
	return 0;
#endif
}

#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
static void board_dc_jack_handle(void)
{
	struct charge_port_info charge_dc_jack;
	uint8_t dedicated_charge_port = board_get_dedicated_charge_port();

	/* System is booted from DC Jack */
	if (board_dc_jack_present()) {
		charge_dc_jack.current = (PD_MAX_POWER_MW * 1000) /
					DC_JACK_MAX_VOLTAGE_MV;
		charge_dc_jack.voltage = DC_JACK_MAX_VOLTAGE_MV;
	} else {
		charge_dc_jack.current = 0;
		charge_dc_jack.voltage = USB_CHARGER_VOLTAGE_MV;
	}

	charge_manager_update_charge(CHARGE_SUPPLIER_DEDICATED,
				dedicated_charge_port, &charge_dc_jack);
}
#endif

void board_dc_jack_interrupt(enum gpio_signal signal)
{
#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
	board_dc_jack_handle();
#endif
}

static void board_charge_init(void)
{
	int port, supplier;
	struct charge_port_info charge_init = {
		.current = 0,
		.voltage = USB_CHARGER_VOLTAGE_MV,
	};

	/* Initialize all charge suppliers to seed the charge manager */
	for (port = 0; port < board_get_charge_port_count(); port++) {
		for (supplier = 0; supplier < CHARGE_SUPPLIER_COUNT; supplier++)
			charge_manager_update_charge(supplier, port,
				&charge_init);
	}

#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
	board_dc_jack_handle();
#endif /* CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0 */
}
DECLARE_HOOK(HOOK_INIT, board_charge_init, HOOK_PRIO_DEFAULT);

int board_set_active_charge_port(int port)
{
	int i;
	/* charge port is a realy physical port */
	int is_real_port = (port >= 0 &&
			port < board_get_charge_port_count());
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
	if (port != board_get_dedicated_charge_port() &&
			board_dc_jack_present()) {
		CPRINTS("DC Jack present, Skip enable p%d", port);
		return EC_ERROR_INVAL;
	}
#endif /* CONFIG_DEDICATED_CHARGE_PORT_COUNT */

	/* Make sure non-charging ports are disabled */
	for (i = 0; i < board_get_usb_pd_port_count(); i++) {
		if (i == port)
			continue;

		board_charging_enable(i, 0);
	}

	/* Enable charging port */
	if (is_typec_port(port))
		board_charging_enable(port, 1);

	CPRINTS("New chg p%d", port);

	return EC_SUCCESS;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	charge_set_input_current_limit(MAX(charge_ma,
				CONFIG_CHARGER_INPUT_CURRENT), charge_mv);
}
