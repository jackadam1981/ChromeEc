/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state_v2.h"
#include "chipset.h"
#include "hooks.h"
#include "usb_mux.h"
#include "system.h"
#include "driver/charger/sm5803.h"
#include "driver/tcpm/it83xx_pd.h"

#include "gpios.h"
#include "sub_board.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.bus_type = EC_BUS_TYPE_EMBEDDED,
		/* TCPC is embedded within EC so no i2c config needed */
		.drv = &it8xxx2_tcpm_drv,
		/* Alert is active-low, push-pull */
		.flags = 0,
	},
	{
		.bus_type = EC_BUS_TYPE_EMBEDDED,
		/* TCPC is embedded within EC so no i2c config needed */
		.drv = &it8xxx2_tcpm_drv,
		/* Alert is active-low, push-pull */
		.flags = 0,
	},
};

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.usb_port = 0,
		.driver = &virtual_usb_mux_driver,
		.hpd_update = &virtual_hpd_update,
	},
	{ /* sub-board */
		.usb_port = 1,
		.driver = &virtual_usb_mux_driver,
		.hpd_update = &virtual_hpd_update,
	},
};

static uint8_t cached_usb_pd_port_count;

__override uint8_t board_get_usb_pd_port_count(void)
{
	if (cached_usb_pd_port_count == 0)
		CPRINTS("USB PD Port count not initialized!");
	return cached_usb_pd_port_count;
}

/*
 * Initialise the USB PD port count, which
 * depends on which sub-board is attached.
 */
static void init_usb_pd_port_count(void)
{
	switch (nissa_get_sb_type()) {
	default:
		cached_usb_pd_port_count = 1;
		break;

	case NISSA_SB_C_A:
	case NISSA_SB_C_LTE:
		cached_usb_pd_port_count = 2;
		break;
	}
}
/*
 * Make sure setup is done after EEPROM is readable.
 */
DECLARE_HOOK(HOOK_INIT, init_usb_pd_port_count, HOOK_PRIO_INIT_I2C + 1);

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	int icl = MAX(charge_ma, CONFIG_CHARGER_INPUT_CURRENT);

	/*
	 * Assume charger overdraws by about 4%, keeping the actual draw
	 * within spec. This adjustment can be changed with characterization
	 * of actual hardware.
	 */
	icl = icl * 96 / 100;
	charge_set_input_current_limit(icl, charge_mv);
}

/* Vconn control for integrated ITE TCPC */
void board_pd_vconn_ctrl(int port, enum usbpd_cc_pin cc_pin, int enabled)
{
	/* Vconn control is only for port 0 */
	if (port)
		return;

	if (cc_pin == USBPD_CC_PIN_1)
		gpio_pin_set_dt(&gpio_en_usb_c0_cc1_vconn, !!enabled);
	else
		gpio_pin_set_dt(&gpio_en_usb_c0_cc2_vconn, !!enabled);
}

int board_is_sourcing_vbus(int port)
{
	return 0;
}

int board_set_active_charge_port(int port)
{
	return EC_SUCCESS;
}

uint16_t tcpc_get_alert_status(void)
{
	return 0;
}

int pd_check_vconn_swap(int port)
{
	/* Allow VCONN swaps if the AP is on. */
	return chipset_in_state(CHIPSET_STATE_ANY_SUSPEND | CHIPSET_STATE_ON);
}

void pd_power_supply_reset(int port)
{
}

int pd_set_power_supply_ready(int port)
{
	return EC_SUCCESS;
}

void board_reset_pd_mcu(void)
{
	/*
	 * TODO(b:147316511): could send a reset command to the TCPC here
	 * if needed.
	 */
}

/*
 * Because the TCPCs and BC1.2 chips share interrupt lines, it's possible
 * for an interrupt to be lost if one asserts the IRQ, the other does the same
 * then the first releases it: there will only be one falling edge to trigger
 * the interrupt, and the line will be held low. We handle this by running a
 * deferred check after a falling edge to see whether the IRQ is still being
 * asserted. If it is, we assume an interrupt may have been lost and we need
 * to poll each chip for events again.
 */
#define USBC_INT_POLL_DELAY_US 5000

static void poll_c0_int(void);
DECLARE_DEFERRED(poll_c0_int);
static void poll_c1_int(void);
DECLARE_DEFERRED(poll_c1_int);

static void usbc_interrupt_trigger(int port)
{
	schedule_deferred_pd_interrupt(port);
	task_set_event(USB_CHG_PORT_TO_TASK_ID(port), USB_CHG_EVENT_BC12);
}

#define USBC_INT_POLL_DATA(port) poll_c ## port ## _int_data
#define USBC_INT_POLL(port)						    \
	static void poll_c ## port ## _int (void)			    \
	{								    \
		if (!gpio_pin_get_dt(&gpio_usb_c ## port ## _int_odl)) { \
			usbc_interrupt_trigger(port);			    \
			hook_call_deferred(&USBC_INT_POLL_DATA(port),	    \
					   USBC_INT_POLL_DELAY_US);	    \
		}							    \
	}

USBC_INT_POLL(0)
USBC_INT_POLL(1)

void usb_c0_interrupt(const struct device *port,
		      struct gpio_callback *cb,
		      gpio_port_pins_t pins)
{
	/*
	 * We've just been called from a falling edge, so there's definitely
	 * no lost IRQ right now. Cancel any pending check.
	 */
	hook_call_deferred(&USBC_INT_POLL_DATA(0), -1);
	/* Trigger polling of TCPC and BC1.2 in respective tasks */
	usbc_interrupt_trigger(0);
	/* Check for lost interrupts in a bit */
	hook_call_deferred(&USBC_INT_POLL_DATA(0), USBC_INT_POLL_DELAY_US);
}

void usb_c1_interrupt(const struct device *port,
		      struct gpio_callback *cb,
		      gpio_port_pins_t pins)
{
	hook_call_deferred(&USBC_INT_POLL_DATA(1), -1);
	usbc_interrupt_trigger(1);
	hook_call_deferred(&USBC_INT_POLL_DATA(1), USBC_INT_POLL_DELAY_US);
}

/*
 * Set up one USB port's interrupt handling.
 */
static void usbc_init_interrupt(int port,
				const struct gpio_dt_spec *gpio,
				struct gpio_callback *cb_data,
				gpio_callback_handler_t cb)
{
	int ret;

	gpio_init_callback(cb_data, cb, BIT(gpio->pin));
	gpio_add_callback(gpio->port, cb_data);
	ret = gpio_pin_interrupt_configure_dt(gpio, GPIO_INT_EDGE_FALLING);
	if (ret != 0)
		CPRINTS("USB init interrupt failed on port %d", port);
}

static void usbc_init(void)
{
	static struct gpio_callback c0_callback;
	static struct gpio_callback c1_callback;

	usbc_init_interrupt(0, &gpio_usb_c0_int_odl,
			    &c0_callback,
			    usb_c0_interrupt);
	if (board_get_usb_pd_port_count() == 2)
		usbc_init_interrupt(1, &gpio_usb_c1_int_odl,
				    &c1_callback,
				    usb_c1_interrupt);
}
DECLARE_HOOK(HOOK_INIT, usbc_init, HOOK_PRIO_DEFAULT);
