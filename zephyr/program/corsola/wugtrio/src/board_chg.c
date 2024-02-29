/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "charge_manager.h"
#include "charge_state.h"
#include "gpio.h"
#include "hooks.h"
#include "intelrvp.h"
#include "tcpm/tcpci.h"

#define DC_JACK_MAX_VOLTAGE_MV 19000

bool is_typec_port(int port)
{
#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
	return !(port == DEDICATED_CHARGE_PORT || port == CHARGE_PORT_NONE);
#else
	return !(port == CHARGE_PORT_NONE);
#endif /* CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0 */
}

#define ADP_DEBOUNCE_MS 1000 /* Debounce time for BJ plug/unplug */

static inline int board_adp_present(void)
{
#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
	return gpio_get_level(GPIO_DC_JACK_PRESENT);
#else
	return 0;
#endif /* CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0 */
}

static void adp_connect_deferred(void)
{
#if CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0
	struct charge_port_info charge_adp;

	/* System is booted from DC Jack */
	if (board_adp_present()) {
		charge_adp.current =
			(PD_MAX_POWER_MW * 1000) / DC_JACK_MAX_VOLTAGE_MV;
		charge_adp.voltage = DC_JACK_MAX_VOLTAGE_MV;
	} else {
		charge_adp.current = 0;
		charge_adp.voltage = 0;
	}

	charge_manager_update_charge(CHARGE_SUPPLIER_DEDICATED,
				     DEDICATED_CHARGE_PORT, &charge_adp);
#endif /* CONFIG_DEDICATED_CHARGE_PORT_COUNT > 0 */
}
DECLARE_DEFERRED(adp_connect_deferred);

/* IRQ for BJ plug/unplug. It shouldn't be called if BJ is the power source. */
void adp_connect_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&adp_connect_deferred_data, ADP_DEBOUNCE_MS * MSEC);
}

static void adp_state_init(void)
{
	ASSERT(CHARGE_PORT_ENUM_COUNT == CHARGE_PORT_COUNT);
	/*
	 * Initialize all charge suppliers to 0. The charge manager waits until
	 * all ports have reported in before doing anything.
	 */
	for (int i = 0; i < CHARGE_PORT_COUNT; i++) {
		for (int j = 0; j < CHARGE_SUPPLIER_COUNT; j++)
			charge_manager_update_charge(j, i, NULL);
	}

	/* Report charge state from the barrel jack. */
	adp_connect_deferred();
}
DECLARE_HOOK(HOOK_INIT, adp_state_init, HOOK_PRIO_INIT_CHARGE_MANAGER + 1);

static void board_adp_init(void)
{
	gpio_enable_interrupt(GPIO_DC_JACK_PRESENT);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

__override int board_set_active_charge_port(int port)
{
	int i;
	int is_valid_port =
		(port >= 0 && port < board_get_adjusted_usb_pd_port_count());
	/* adjust the actual port count when not the type-c db connected. */

	if (!is_valid_port && port != CHARGE_PORT_NONE) {
		return EC_ERROR_INVAL;
	}

	if (port == CHARGE_PORT_NONE) {
		CPRINTS("Disabling all charger ports");

		/* Disable all ports. */
		for (i = 0; i < board_get_adjusted_usb_pd_port_count(); i++) {
			/*
			 * Do not return early if one fails otherwise we can
			 * get into a boot loop assertion failure.
			 */
			if (ppc_vbus_sink_enable(i, 0)) {
				CPRINTS("Disabling C%d as sink failed.", i);
			}
		}

		return EC_SUCCESS;
	}

	/* Check if the port is sourcing VBUS. */
	if (ppc_is_sourcing_vbus(port)) {
		CPRINTS("Skip enable C%d", port);
		return EC_ERROR_INVAL;
	}

	CPRINTS("New charge port: C%d", port);

	/*
	 * Turn off the other ports' sink path FETs, before enabling the
	 * requested charge port.
	 */
	for (i = 0; i < board_get_adjusted_usb_pd_port_count(); i++) {
		if (i == port) {
			continue;
		}

		if (ppc_vbus_sink_enable(i, 0)) {
			CPRINTS("C%d: sink path disable failed.", i);
		}
	}

	/* Enable requested charge port. */
	if (is_typec_port(port)) {
        ppc_vbus_sink_enable(port, 1);
		CPRINTS("C%d: sink path enable failed.", port);
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

#ifdef CONFIG_USB_PD_VBUS_MEASURE_ADC_EACH_PORT
enum adc_channel board_get_vbus_adc(int port)
{
	if (port == 0) {
		return ADC_VBUS_C0;
	}
	if (port == 1) {
		return ADC_VBUS_C1;
	}
	CPRINTSUSB("Unknown vbus adc port id: %d", port);
	return ADC_VBUS_C0;
}
#endif /* CONFIG_USB_PD_VBUS_MEASURE_ADC_EACH_PORT */
