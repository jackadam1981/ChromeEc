/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_state.h"
#include "charger.h"
#include "chipset.h"
#include "driver/ppc/syv682x_public.h"
#include "driver/tcpm/it83xx_pd.h"
#include "driver/tcpm/ps8xxx_public.h"
#include "driver/tcpm/tcpci.h"
#include "gpio.h"
#include "hooks.h"
#include "nissa_sub_board.h"
#include "system.h"
#include "usb_mux.h"
#include "usbc_ppc.h"
#include "usb_pd.h"
#include "usb_tc_sm.h"

#include <zephyr/logging/log.h>

#define CPRINTSUSB(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTFUSB(format, args...) cprintf(CC_USBCHARGE, format, ##args)

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

/* Vconn control for integrated ITE TCPC */
void board_pd_vconn_ctrl(int port, enum usbpd_cc_pin cc_pin, int enabled)
{
	/*
	 * We ignore the cc_pin and PPC vconn because polarity and PPC vconn
	 * should already be set correctly in the PPC driver via the pd
	 * state machine.
	 */
}

enum usbc_port { USBC_PORT_C0 = 0, USBC_PORT_C1, USBC_PORT_COUNT };

/* Used by USB charger task with CONFIG_USB_PD_5V_EN_CUSTOM */
int board_is_sourcing_vbus(int port)
{
	return ppc_is_sourcing_vbus(port);
}

int board_vbus_source_enabled(int port)
{
	return ppc_is_sourcing_vbus(port);
}

int board_set_active_charge_port(int port)
{
	int is_valid_port = board_is_usb_pd_port_present(port);
	int i;

	if (port == CHARGE_PORT_NONE) {
		CPRINTSUSB("Disabling all charger ports");

		/* Disable all ports. */
		for (i = 0; i < ppc_cnt; i++) {
			/*
			 * Do not return early if one fails otherwise we can
			 * get into a boot loop assertion failure.
			 */
			if (ppc_vbus_sink_enable(i, 0))
				CPRINTSUSB("Disabling C%d as sink failed.", i);
		}

		return EC_SUCCESS;
	} else if (!is_valid_port) {
		return EC_ERROR_INVAL;
	}

	/* Check if the port is sourcing VBUS. */
	if (board_is_sourcing_vbus(port)) {
		CPRINTFUSB("Skip enable C%d", port);
		return EC_ERROR_INVAL;
	}

	CPRINTSUSB("New charge port: C%d", port);

	/*
	 * Turn off the other ports' sink path FETs, before enabling the
	 * requested charge port.
	 */
	for (i = 0; i < ppc_cnt; i++) {
		if (i == port)
			continue;

		if (ppc_vbus_sink_enable(i, 0))
			CPRINTSUSB("C%d: sink path disable failed.", i);
	}

	/* Enable requested charge port. */
	if (ppc_vbus_sink_enable(port, 1)) {
		CPRINTSUSB("C%d: sink path enable failed.", port);
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

void pd_power_supply_reset(int port)
{
	/* Disable VBUS. */
	ppc_vbus_source_enable(port, 0);

	/* Enable discharge if we were previously sourcing 5V */
	if (IS_ENABLED(CONFIG_USB_PD_DISCHARGE))
		pd_set_vbus_discharge(port, 1);

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	if (port >= CONFIG_USB_PD_PORT_MAX_COUNT) {
		return EC_ERROR_INVAL;
	}

	/* Disable charging. */
	rv = ppc_vbus_sink_enable(port, 0);
	if (rv) {
		LOG_WRN("C%d failed to disable sinking: %d", port, rv);
		return rv;
	}
	if (IS_ENABLED(CONFIG_USB_PD_DISCHARGE)) {
		pd_set_vbus_discharge(port, 0);
	}

	/* Provide Vbus. */
	rv = ppc_vbus_source_enable(port, 1);
	if (rv) {
		LOG_WRN("C%d failed to enable VBUS sourcing: %d", port, rv);
		return rv;
	}

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);

	return EC_SUCCESS;
}

__override int pd_snk_is_vbus_provided(int port)
{
	return ppc_is_vbus_present(port);
}

__override void typec_set_source_current_limit(int port, enum tcpc_rp_value rp)
{
	int rv;
	const int current = rp == TYPEC_RP_3A0 ? 3000 : 1500;

	rv = ppc_set_vbus_source_current_limit(port, rp);
	if (rv != EC_SUCCESS) {
		LOG_WRN("Failed to set source ilimit on port %d to %d: %d",
			port, current, rv);
	}
}

/* LCOV_EXCL_START function does nothing, but is required for build */
void board_reset_pd_mcu(void)
{
	/*
	 * Do nothing. The integrated TCPC for C0 lacks a dedicated reset
	 * command, and C1 (if present) doesn't have a reset pin connected
	 * to the EC.
	 */
}
/* LCOV_EXCL_STOP */

// #define BATT_LVL_CURRENT_LIMITED 51 /* % */
#define DISCHARGE_CURRENT_LIMITED (-1000) /* mA */

#define PDO_FIXED_FLAGS \
	(PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_COMM_CAP)

static bool current_limited;

static const uint32_t pd_src_pdo_1A5[] = {
	PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};

static const uint32_t pd_src_pdo_3A[] = {
	PDO_FIXED(5000, 3000, PDO_FIXED_FLAGS),
};

int dpm_get_source_pdo(const uint32_t **src_pdo, const int port)
{
	if (current_limited) {
		*src_pdo = pd_src_pdo_1A5;
		return ARRAY_SIZE(pd_src_pdo_1A5);
	}

	*src_pdo = pd_src_pdo_3A;

	return ARRAY_SIZE(pd_src_pdo_3A);
}

static void update_src_pdo_deferred(void);
DECLARE_DEFERRED(update_src_pdo_deferred);
static void update_src_pdo_deferred(void)
{
	static int get_curr[4];
	static int i;
	static bool limited_status;
	int j;
	int hold_flag,release_flag;
	const struct batt_params *batt = charger_current_battery_params();
	// struct battery_static_info *bs = &battery_static[BATT_IDX_MAIN];

	// CPRINTSUSB("%s", bs->model_ext);

	get_curr[i++] = batt->current;

	if (i >= 4) {
		i = 0;
	}

	hold_flag = 0;
	release_flag = 0;

	for (j = 0;j < 4;j++) {
		// printk("--get_curr_%d=%d\n",j,get_curr[j]);
		if (get_curr[j] < DISCHARGE_CURRENT_LIMITED) {
			hold_flag++;
		} else if (get_curr[j] < 0) {
			release_flag++;
		} else {
			CPRINTSUSB("Battery is not discharged");
			hook_call_deferred(&update_src_pdo_deferred_data, 500 * MSEC);
			return;
		}
	}

	if (hold_flag == 4) {
		for (j = 0; j < board_get_usb_pd_port_count(); j++) {
			if (!tc_is_attached_src(j)) {
				break;
			}
		}

		if (j == CONFIG_USB_PD_PORT_MAX_COUNT) {
			current_limited = true;

			if (limited_status == current_limited) {
				hook_call_deferred(&update_src_pdo_deferred_data, 500 * MSEC);
				return;
			}
			CPRINTSUSB("Battery over current!");

			for (j = 0; j < board_get_usb_pd_port_count(); j++) {
				CPRINTSUSB("Set C%d src pdo 1A5", j);
				typec_set_source_current_limit(j, TYPEC_RP_1A5);
				pd_update_contract(j);
			}
			limited_status = current_limited;
		}
	} else if (release_flag == 4) {
		current_limited = false;

		if (limited_status == current_limited) {
			hook_call_deferred(&update_src_pdo_deferred_data, 500 * MSEC);
			return;
		}
		CPRINTSUSB("Release current limited");

		for (j = 0; j < board_get_usb_pd_port_count(); j++) {
			if (tc_is_attached_src(j)) {
				CPRINTSUSB("Set C%d src pdo 3A", j);
				typec_set_source_current_limit(j, TYPEC_RP_3A0);
				pd_update_contract(j);
			}
		}
		limited_status = current_limited;
	}

	hook_call_deferred(&update_src_pdo_deferred_data, 500 * MSEC);
}

static void check_batt_current(void)
{
	/* Deferred 5s to avoid pd state conflict */
	hook_call_deferred(&update_src_pdo_deferred_data, 5 * SECOND);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, check_batt_current, HOOK_PRIO_DEFAULT);

static void stop_check_batt(void)
{
	// gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_prochot_odl), 1);
	current_limited = false;
	hook_call_deferred(&update_src_pdo_deferred_data, -1);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, stop_check_batt, HOOK_PRIO_DEFAULT);
