/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#include "charge_manager.h"
#include "charge_state_v2.h"
#include "charger.h"
#include "compile_time_macros.h"
#include "console.h"
#include "driver/charger/isl9241.h"
#include "gpio.h"
#include "hooks.h"
#include "stdbool.h"
#include "usbc_ppc.h"
#include "usb_pd.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

/* Charger Chip Configuration */
const struct charger_config_t chg_chips[] = {
	{
		.i2c_port = I2C_PORT_CHARGER,
		.i2c_addr_flags = ISL9241_ADDR_FLAGS,
		.drv = &isl9241_drv,
	},
};
BUILD_ASSERT(ARRAY_SIZE(chg_chips) == CHARGER_NUM);

static int board_disable_bj_port(void)
{
	gpio_set_level(GPIO_EN_PPVAR_BJ_ADP, 0);
	/* If the current port is BJ, disable bypass mode. */
	if (charge_manager_get_supplier() == CHARGE_SUPPLIER_DEDICATED)
		return charger_enable_bypass_mode(0, 0);

	CPRINTS("BJ power is disabled");

	return EC_SUCCESS;
}

static int board_enable_bj_port(void)
{
	if (gpio_get_level(GPIO_BJ_ADP_PRESENT_ODL))
		return EC_ERROR_INVAL;
	gpio_set_level(GPIO_EN_PPVAR_BJ_ADP, 1);

	CPRINTS("BJ power is enabled");

	return charger_enable_bypass_mode(0, 1);
}

static int board_throttle_ap_gpu(bool enable)
{
	int rv = EC_SUCCESS;

	if (!chipset_in_state(CHIPSET_STATE_ON))
		return EC_SUCCESS;

	CPRINTS("%s to %s AP & GPU (%d)", rv ? "Failed" : "Succeeded",
			enable ? "throttle" : "unthrottle", rv);

	return rv;
}

static int board_disable_vbus_sink(int port)
{
	int i, r, rv = EC_SUCCESS;

	for (i = 0; i < ppc_cnt; i++) {
		/* port == -1 would disable all. */
		if (i == port)
			continue;
		/*
		 * Do not return early if one fails otherwise we can get into a
		 * boot loop assertion failure.
		 */
		r = ppc_vbus_sink_enable(i, 0);
		CPRINTS("C%d: Disabling sink path %s (%d).",
			i, r ? "failed" : "succeeded", r);
		rv |= r;
	}

	return rv;
}

/*
 * It should also work on POR with/without a battery:
 *
 * 1. EC gathers power info of all ports.
 * 1. Identify the highest power port.
 * 1. If
 *    1. battery soc = 0% --> Exit
 *    1. BJ_ADP_PRESENT_ODL = 1 --> Exit
 *    1. highest power port == active port --> Exit
 * 1. If
 *    1. in S0, throttle AP & GPU to the DC rating.
 * 1. Turn off the current active port.
 * 1. Turn on the highest power port.
 * 1. If
 *    1. in S0, throttle AP & GPU back.
 *
 * TODO: Are the following cases covered?
 * 1. Two AC adapters are plugged. Then, the active adapter is removed.
 *
 * TODO: Recover from incomplete execution:
 * 1. Failed to turn on/off PPC.
 */
int board_set_active_charge_port(int port)
{
	enum charge_supplier supplier = charge_manager_get_supplier();
	int rv;

	CPRINTS("Changing charge port to %d (current port=%d supplier=%d)",
		port, charge_manager_get_active_charge_port(), supplier);

	if (port == CHARGE_PORT_NONE) {
		CPRINTS("Disabling all charger ports");

		/*
		 * TODO: Does this work timely? board_set_active_charge_port
		 * isn't the fastest responder for loss of AC. We may need to
		 * do this on EC_GPU_ACOFF_ODL IRQ.
		 */
		//board_throttle_ap_gpu(1);

		board_disable_bj_port();
		board_disable_vbus_sink(-1);

		return EC_SUCCESS;
	}

	if (port < 0 || CHARGE_PORT_COUNT <= port)
		return EC_ERROR_INVAL;

	if (port == charge_manager_get_active_charge_port())
		return EC_SUCCESS;

	/* Don't charge from a USBC source port */
	if (board_vbus_source_enabled(port)) {
		CPRINTS("Skip enable C%d", port);
		return EC_ERROR_INVAL;
	}

	/*
	 * We need to check the battery if we're switching a source port. If
	 * we're just starting up or no AC was previously plugged, we shouldn't
	 * check the battery. Both cases can be caught by supplier == NONE.
	 */
	if (supplier != CHARGE_SUPPLIER_NONE) {
		struct batt_params batt;

		battery_get_params(&batt);
		if (batt.state_of_charge < 1)
			return EC_ERROR_NOT_POWERED;
		rv = board_throttle_ap_gpu(1);
		if (rv)
			return rv;
	}

	/*
	 * Turn off the other ports' sink path FETs before enabling the
	 * requested charge port.
	 */
	if (port == CHARGE_PORT_TYPEC0 || port == CHARGE_PORT_TYPEC1) {
		/*
		 * BJ port is on POR. So, we need to turn it off even if we're
		 * not previously on BJ.
		 */
		board_disable_bj_port();
		if (board_disable_vbus_sink(port))
			return EC_ERROR_UNCHANGED;

		/* Enable requested USBC charge port. */
		if (ppc_vbus_sink_enable(port, 1)) {
			CPRINTS("C%d: sink path enable failed.", port);
			return EC_ERROR_UNKNOWN;
		}
	} else if (port == CHARGE_PORT_BARRELJACK) {
		/*
		 * We can't proceed unless both ports are successfully
		 * disconnected as sources.
		 */
		if (board_disable_vbus_sink(-1))
			return EC_ERROR_UNKNOWN;
		board_enable_bj_port();
	}

	/*
	 * Switching a port is complete. Throttle back AP & GPU.
	 */
	if (supplier != CHARGE_SUPPLIER_NONE)
		board_throttle_ap_gpu(0);

	CPRINTS("New charger p%d", port);

	return EC_SUCCESS;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	charge_set_input_current_limit(MAX(charge_ma,
					   CONFIG_CHARGER_INPUT_CURRENT),
				       charge_mv);
}

static const struct charge_port_info bj_power = {
	/* 150W (also default) */
	.voltage = 19500,
	.current = 7700,
};

#define ADP_DEBOUNCE_MS		1000  /* Debounce time for BJ plug/unplug */
/* Debounced connection state of the barrel jack */
static int8_t adp_connected = -1;
static void adp_connect_deferred(void)
{
	const struct charge_port_info *pi = NULL;
	int connected = !gpio_get_level(GPIO_BJ_ADP_PRESENT_ODL);

	/* Debounce */
	if (connected == adp_connected)
		return;

	if (connected)
		pi = &bj_power;

	charge_manager_update_charge(CHARGE_SUPPLIER_DEDICATED,
				     DEDICATED_CHARGE_PORT, pi);
	adp_connected = connected;
	CPRINTS("BJ %s", connected ? "connected" : "disconnected");
}
DECLARE_DEFERRED(adp_connect_deferred);

/* IRQ for BJ plug/unplug. It shouldn't be called if BJ is the power source. */
void adp_connect_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&adp_connect_deferred_data, ADP_DEBOUNCE_MS * MSEC);
}

static void adp_state_init(void)
{
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
