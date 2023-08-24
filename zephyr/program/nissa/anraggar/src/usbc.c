/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "chipset.h"
// #include "driver/charger/sm5803.h"
#include "driver/tcpm/it83xx_pd.h"
#include "driver/tcpm/ps8xxx_public.h"
#include "driver/tcpm/tcpci.h"
#include "hooks.h"
#include "system.h"
#include "usb_mux.h"

#include <zephyr/logging/log.h>

#include <ap_power/ap_power.h>

#include "driver/charger/bq25710.h"
#include "usbc_ppc.h"
#include "charge_manager.h"
#include "charger.h"
#include "common.h"
#include "usb_pd.h"
#include "charge_ramp.h"
#include "gpio.h"
#include "gpio/gpio.h"
#include "usb_common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "ec_commands.h"
#include "ioexpander.h"
#include "power_signals.h"
#include "util.h"
#include "usbc_ppc.h"

#define CPRINTSUSB(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTFUSB(format, args...) cprintf(CC_USBCHARGE, format, ##args)

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

/* Vconn control for integrated ITE TCPC */
void board_pd_vconn_ctrl(int port, enum usbpd_cc_pin cc_pin, int enabled)
{
	/* Vconn control is only for port 0 */
	if (port)
		return;

	if (cc_pin == USBPD_CC_PIN_1)
		gpio_pin_set_dt(
			GPIO_DT_FROM_NODELABEL(gpio_en_usb_c0_cc1_vconn),
			!!enabled);
	else
		gpio_pin_set_dt(
			GPIO_DT_FROM_NODELABEL(gpio_en_usb_c0_cc2_vconn),
			!!enabled);
}

__override bool pd_check_vbus_level(int port, enum vbus_level level)
{
	// return sm5803_check_vbus_level(port, level);
	return ppc_is_vbus_present(port);
}

/*
 * Putting chargers into LPM when in suspend reduces power draw by about 8mW
 * per charger, but also seems critical to correct operation in source mode:
 * if chargers are not in LPM when a sink is first connected, VBUS sourcing
 * works even if the partner is later removed (causing LPM entry) and
 * reconnected (causing LPM exit). If in LPM initially, sourcing VBUS
 * consistently causes the charger to report (apparently spurious) overcurrent
 * failures.
 *
 * In short, this is important to making things work correctly but we don't
 * understand why.
 */
static void board_chargers_suspend(struct ap_power_ev_callback *const cb,
				   const struct ap_power_ev_data data)
{
	// void (*fn)(int chgnum);

	// switch (data.event) {
	// case AP_POWER_SUSPEND:
	// 	fn = sm5803_enable_low_power_mode;
	// 	break;
	// case AP_POWER_RESUME:
	// 	fn = sm5803_disable_low_power_mode;
	// 	break;
	// /* LCOV_EXCL_START can only happen if init doesn't match these cases */
	// default:
	// 	LOG_WRN("%s: power event %d is not recognized", __func__,
	// 		data.event);
	// 	return;
	// 	/* LCOV_EXCL_STOP */
	// }

	// fn(CHARGER_PRIMARY);
	// if (board_get_charger_chip_count() > 1)
	// 	fn(CHARGER_SECONDARY);
}

static int board_chargers_suspend_init(void)
{
	static struct ap_power_ev_callback cb = {
		.handler = board_chargers_suspend,
		.events = AP_POWER_SUSPEND | AP_POWER_RESUME,
	};
	ap_power_ev_add_callback(&cb);
	return 0;
}
SYS_INIT(board_chargers_suspend_init, APPLICATION, 0);

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
	if (ppc_is_sourcing_vbus(port)) {
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

uint16_t tcpc_get_alert_status(void)
{
	/*
	 * TCPC 0 is embedded in the EC and processes interrupts in the chip
	 * code (it83xx/intc.c). This function only needs to poll port C1 if
	 * present.
	 */
	uint16_t status = 0;
	int regval;

	/* Is the C1 port present and its IRQ line asserted? */
	if (board_get_usb_pd_port_count() == 2 &&
	    !gpio_pin_get_dt(GPIO_DT_FROM_ALIAS(gpio_usb_c1_int_odl))) {
		/*
		 * C1 IRQ is shared between BC1.2 and TCPC; poll TCPC to see if
		 * it asserted the IRQ.
		 */
		if (!tcpc_read16(1, TCPC_REG_ALERT, &regval)) {
			if (regval)
				status = PD_STATUS_TCPC_ALERT_1;
		}
	}

	return status;
}

void pd_power_supply_reset(int port)
{
	int prev_en;

	prev_en = ppc_is_sourcing_vbus(port);

	/* Disable VBUS. */
	ppc_vbus_source_enable(port, 0);

	/* Enable discharge if we were previously sourcing 5V */
	if (prev_en)
		pd_set_vbus_discharge(port, 1);

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	/* Disable charging. */
	rv = ppc_vbus_sink_enable(port, 0);
	if (rv)
		return rv;

	pd_set_vbus_discharge(port, 0);

	/* Provide Vbus. */
	rv = ppc_vbus_source_enable(port, 1);
	if (rv)
		return rv;

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);

	return EC_SUCCESS;
}

int board_vbus_source_enabled(int port)
{
	/* BJ port is always sink. */
	if (port >= CONFIG_USB_PD_PORT_MAX_COUNT)
		return 0;
	return ppc_is_sourcing_vbus(port);
}

__override void typec_set_source_current_limit(int port, enum tcpc_rp_value rp)
{
	int rv;
	const int current = rp == TYPEC_RP_3A0 ? 3000 : 1500;

	rv = charger_set_otg_current_voltage(0, current, 5000);
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

#define INT_RECHECK_US 5000

/* C0 interrupt line shared by BC 1.2 and charger */

static void check_c0_line(void);
DECLARE_DEFERRED(check_c0_line);

static void notify_c0_chips(void)
{
	usb_charger_task_set_event(0, USB_CHG_EVENT_BC12);
	// sm5803_interrupt(0);
}

static void check_c0_line(void)
{
	/*
	 * If line is still being held low, see if there's more to process from
	 * one of the chips
	 */
	if (!gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_c0_int_odl))) {
		notify_c0_chips();
		hook_call_deferred(&check_c0_line_data, INT_RECHECK_US);
	}
}

void usb_c0_interrupt(enum gpio_signal s)
{
	/* Cancel any previous calls to check the interrupt line */
	hook_call_deferred(&check_c0_line_data, -1);

	/* Notify all chips using this line that an interrupt came in */
	notify_c0_chips();

	/* Check the line again in 5ms */
	hook_call_deferred(&check_c0_line_data, INT_RECHECK_US);
}

/* C1 interrupt line shared by BC 1.2, TCPC, and charger */
/* LCOV_EXCL_START schedule_deferred_pd_interrupt() is untestable */
void usb_c1_interrupt(enum gpio_signal s)
{
	/* Charger and BC1.2 are handled in board_process_pd_alert */
	schedule_deferred_pd_interrupt(1);
}
/* LCOV_EXCL_STOP */

/*
 * Handle charger interrupts in the PD task. Not doing so can lead to a priority
 * inversion where we fail to respond to TCPC alerts quickly enough because we
 * don't get another edge on a shared IRQ until the other interrupt is cleared
 * (or the IRQ is polled again), which happens in lower-priority tasks: the
 * high-priority type-C handler is thus blocked on the lower-priority one(s).
 *
 * To avoid that, we run charger and BC1.2 interrupts synchronously alongside
 * PD interrupts so they have the same priority.
 */
void board_process_pd_alert(int port)
{
	/*
	 * Port 0 doesn't use an external TCPC, so its interrupts don't need
	 * this special handling.
	 */
	// if (port != 1)
	// 	return;

	// if (!gpio_pin_get_dt(GPIO_DT_FROM_ALIAS(gpio_usb_c1_int_odl))) {
	// 	sm5803_handle_interrupt(port);
	// 	usb_charger_task_set_event_sync(1, USB_CHG_EVENT_BC12);
	// }
	// /*
	//  * Immediately schedule another TCPC interrupt if it seems we haven't
	//  * cleared all pending interrupts.
	//  */
	// if (!gpio_pin_get_dt(GPIO_DT_FROM_ALIAS(gpio_usb_c1_int_odl)))
	// 	schedule_deferred_pd_interrupt(port);
}

int pd_snk_is_vbus_provided(int port)
{
	int chg_det = 0;

	// sm5803_get_chg_det(port, &chg_det);
	chg_det = ppc_is_sourcing_vbus(port);

	return chg_det;
}

int board_is_vbus_too_low(int port, enum chg_ramp_vbus_state ramp_state)
{
	int voltage;

	if (charger_get_vbus_voltage(port, &voltage))
		voltage = 0;


	return 0;
}
