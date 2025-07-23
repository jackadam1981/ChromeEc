/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "board.h"
#include "charge_manager.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "power.h"
#include "system.h"
#include "tcpm/tcpci.h"
#include "uart.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"

#include <zephyr/logging/log.h>

/*
 * Enable interrupts
 */
test_export_static void board_bj_init(void)
{
	/*
	 * Enable USB-C interrupts.
	 */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_bj_adp_present));
}
DECLARE_HOOK(HOOK_INIT, board_bj_init, HOOK_PRIO_DEFAULT);

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

/******************************************************************************/
/*
 * Since dirks has no battery, it must source all of its power from either
 * USB-C or the barrel jack (preferred). Dirks operates in continuous safe
 * mode (charge_manager_leave_safe_mode() will never be called), which
 * modifies port selection as follows:
 *
 * - Dual-role / dedicated capability of the port partner is ignored.
 * - Charge ceiling on PD voltage transition is ignored.
 * - CHARGE_PORT_NONE will never be selected.
 */

/* List of BJ adapters */
enum bj_adapter {
	BJ_NONE,
	BJ_65W_19V,
};

/* Barrel-jack power adapter ratings. */
static const struct charge_port_info bj_adapters[] = {
	[BJ_NONE] = { .current = 0, .voltage = 0 },
	[BJ_65W_19V] = { .current = 3420, .voltage = 19000 },
};
#define BJ_ADP_RATING_DEFAULT BJ_65W_19V /* BJ power ratings default */
#define ADP_DEBOUNCE_MS 1000 /* Debounce time for BJ plug/unplug */

/* Debounced connection state of the barrel jack */
static int8_t bj_adp_connected = -1;
static void adp_connect_deferred(void)
{
	const struct charge_port_info *pi;
	int connected =
		gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_bj_adp_present));

	/* Debounce */
	if (connected == bj_adp_connected)
		return;

	if (connected) {
		pi = &bj_adapters[BJ_ADP_RATING_DEFAULT];
	} else {
		/* No barrel-jack, zero out this power supply */
		pi = &bj_adapters[BJ_NONE];
	}
	/* This will result in a call to board_set_active_charge_port */
	charge_manager_update_charge(CHARGE_SUPPLIER_DEDICATED,
				     DEDICATED_CHARGE_PORT, pi);
	bj_adp_connected = connected;
}
DECLARE_DEFERRED(adp_connect_deferred);

/* IRQ for BJ plug/unplug. It shouldn't be called if BJ is the power source. */
void adp_connect_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&adp_connect_deferred_data,
			   ADP_DEBOUNCE_MS * USEC_PER_MSEC);
}

int board_set_active_charge_port(int port)
{
	const int active_port = charge_manager_get_active_charge_port();

	LOG_INF("Requested charge port change to %d", port);

	if (port < 0 || CHARGE_PORT_COUNT <= port)
		return EC_ERROR_INVAL;

	if (port == active_port)
		return EC_SUCCESS;

	/* Don't sink from a source port */
	if (board_vbus_source_enabled(port))
		return EC_ERROR_INVAL;

	if (!chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
		int bj_requested;

		if (charge_manager_get_active_charge_port() != CHARGE_PORT_NONE)
			/* Change is only permitted while the system is off */
			return EC_ERROR_INVAL;

		/*
		 * Current setting is no charge port but the AP is on, so the
		 * charge manager is out of sync (probably because we're
		 * reinitializing after sysjump). Reject requests that aren't
		 * in sync with our outputs.
		 */
		bj_requested = port == CHARGE_PORT_BARRELJACK;
		if (bj_adp_connected != bj_requested)
			return EC_ERROR_INVAL;
	}

	LOG_INF("New charger p%d", port);

	switch (port) {
	case CHARGE_PORT_TYPEC0:
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_ppvar_bj_adp_od),
				0);
		ppc_vbus_sink_enable(USBC_PORT_C0, 1);
		break;
	case CHARGE_PORT_BARRELJACK:
		/* Make sure BJ adapter is sourcing power */
		if (!gpio_pin_get_dt(
			    GPIO_DT_FROM_NODELABEL(gpio_bj_adp_present)))
			return EC_ERROR_INVAL;
		ppc_vbus_sink_enable(USBC_PORT_C0, 0);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_ppvar_bj_adp_od),
				1);
		break;
	default:
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

static void board_charge_manager_init(void)
{
	enum charge_port port;

	/*
	 * Initialize all charge suppliers to 0. The charge manager waits until
	 * all ports have reported in before doing anything.
	 */
	for (int i = 0; i < CHARGE_PORT_COUNT; i++) {
		for (int j = 0; j < CHARGE_SUPPLIER_COUNT; j++)
			charge_manager_update_charge(j, i, NULL);
	}

	port = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_bj_adp_present)) ?
		       CHARGE_PORT_BARRELJACK :
		       CHARGE_PORT_TYPEC0;
	LOG_INF("Power source is p%d (%s)", port,
		port == CHARGE_PORT_TYPEC0 ? "USB-C" : "BJ");

	/* Initialize the power source supplier */
	switch (port) {
	case CHARGE_PORT_TYPEC0:
		typec_set_input_current_limit(port, 3000, 5000);
		break;
	case CHARGE_PORT_BARRELJACK:
		charge_manager_update_charge(
			CHARGE_SUPPLIER_DEDICATED, DEDICATED_CHARGE_PORT,
			&bj_adapters[BJ_ADP_RATING_DEFAULT]);
		break;
	}

	/* Report charge state from the barrel jack. */
	adp_connect_deferred();
}
DECLARE_HOOK(HOOK_INIT, board_charge_manager_init,
	     HOOK_PRIO_INIT_CHARGE_MANAGER + 1);

/*
 * Dirks can be powered by BJ adapter and USBC adapter. We
 * can check if the power is good by the method:
 * BJ adapter: check the present pin existence
 * USBC adapter: check if the actual VBUS is more than 5% less of the
 * requested VBUS.
 */

__override bool board_is_power_good(void)
{
	int active_port = charge_manager_get_active_charge_port();

	if (active_port == CHARGE_PORT_BARRELJACK) {
		if (!gpio_pin_get_dt(
			    GPIO_DT_FROM_NODELABEL(gpio_bj_adp_present))) {
			return false;
		}
	} else if (active_port == CHARGE_PORT_TYPEC0) {
		int voltage = charge_manager_get_charger_voltage();
		if (adc_read_channel(ADC_VBUS) < voltage * 0.95) {
			return false;
		}
	} else {
		/* Charge port should be one of BJ or USBC */
		return false;
	}

	return true;
}
