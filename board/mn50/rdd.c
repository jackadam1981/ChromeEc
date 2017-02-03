/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "case_closed_debug.h"
#include "console.h"
#include "device_state.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "rbox.h"
#include "rdd.h"
#include "registers.h"
#include "system.h"
#include "uartn.h"
#include "usb_api.h"
#include "usb_i2c.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

static int keep_ccd_enabled;
static int ec_uart_enabled, enable_usb_wakeup;
static int usb_is_initialized;

struct uart_config {
	const char *name;
	enum device_type device;
	int tx_signal;
};

static struct uart_config uarts[] = {
	[UART_AP] = {"AP", DEVICE_AP, GC_PINMUX_UART1_TX_SEL},
	[UART_EC] = {"EC", DEVICE_EC, GC_PINMUX_UART2_TX_SEL},
};

static int ccd_is_enabled(void)
{
	return ccd_get_mode() == CCD_MODE_ENABLED;
}

int is_utmi_wakeup_allowed(void)
{
	return enable_usb_wakeup;
}


/* If the UART TX is enabled the pinmux select will have a non-zero value */
int uartn_enabled(int uart)
{
	if (uart == UART_AP)
		return GREAD(PINMUX, DIOA7_SEL);
	return GREAD(PINMUX, DIOB5_SEL);
}

/**
 * Connect the UART pin to the given signal
 *
 * @param uart		the uart peripheral number
 * @param signal	the pinmux selector value for the gpio or peripheral
 *			function. 0 to disable the output.
 */
static void uart_select_tx(int uart, int signal)
{
	if (uart == UART_AP) {
		GWRITE(PINMUX, DIOA7_SEL, signal);
	} else {
		GWRITE(PINMUX, DIOB5_SEL, signal);

		/* Remove the pulldown when we are driving the signal */
		GWRITE_FIELD(PINMUX, DIOB5_CTL, PD, signal ? 0 : 1);
	}
}

void uartn_tx_connect(int uart)
{
	if (uart == UART_EC && !ec_uart_enabled)
		return;

	if (!ccd_is_enabled())
		return;

	uart_select_tx(uart, uarts[uart].tx_signal);
}

void uartn_tx_disconnect(int uart)
{
	/* Disconnect the TX pin from UART peripheral */
	uart_select_tx(uart, 0);
}

void rdd_attached(void)
{
	if (ccd_is_enabled())
		return;

	/* Indicate case-closed debug mode (active low) */
	gpio_set_level(GPIO_CCD_MODE_L, 0);

	/* Enable CCD */
	ccd_set_mode(CCD_MODE_ENABLED);

	enable_usb_wakeup = 1;

	uartn_tx_connect(UART_AP);
}

void rdd_detached(void)
{
	if (keep_ccd_enabled)
		return;

	/* Disconnect from AP and EC UART TX peripheral from gpios */
	uartn_tx_disconnect(UART_EC);
	uartn_tx_disconnect(UART_AP);

	/* Done with case-closed debug mode */
	gpio_set_level(GPIO_CCD_MODE_L, 1);

	enable_usb_wakeup = 0;
	ec_uart_enabled = 0;

	/* Disable CCD */
	ccd_set_mode(CCD_MODE_DISABLED);
}

void ccd_phy_init(int enable_ccd)
{
	/*
	 * For boards that have one phy connected to the AP and one to the
	 * external port PHY0 is for the AP and PHY1 is for CCD.
	 */
	uint32_t which_phy = enable_ccd ? USB_SEL_PHY1 : USB_SEL_PHY0;

	/*
	 * TODO: if both PHYs are connected to the external port select the
	 * PHY based on the detected polarity
	 */
	usb_select_phy(which_phy);

	/*
	 * If the usb is going to be initialized on the AP PHY, but the AP is
	 * off, wait until HOOK_CHIPSET_RESUME to initialize usb.
	 */
	if (!enable_ccd) {
		usb_is_initialized = 0;
		return;
	}

	/*
	 * If the board has the non-ccd phy connected to the AP initialize the
	 * phy no matter what. Otherwise only initialize the phy if ccd is
	 * enabled.
	 */
	if (board_has_ap_usb() || enable_ccd) {
		usb_init();
		usb_is_initialized = 1;
	}
}

static void clear_keepalive(void)
{
	keep_ccd_enabled = 0;
	ccprintf("Cleared CCD keepalive\n");
}

void ccd_force_enable(void)
{
	/* Make sure ccd is enabled */
	if (!ccd_is_enabled())
		rdd_attached();
	keep_ccd_enabled = 1;
}

static int command_ccd(int argc, char **argv)
{
	int val;

	if (argc > 1) {
		if (!parse_bool(argv[argc - 1], &val))
			return argc == 2 ? EC_ERROR_PARAM1 : EC_ERROR_PARAM2;

		if (!strcasecmp("uart", argv[1])) {
			if (val) {
				ec_uart_enabled = 1;
				uartn_tx_connect(UART_EC);
			} else {
				ec_uart_enabled = 0;
				uartn_tx_disconnect(UART_EC);
			}
		} else if (!strcasecmp("i2c", argv[1])) {
			if (val)
				usb_i2c_board_enable();
			else
				usb_i2c_board_disable();
		} else if (!strcasecmp("keepalive", argv[1])) {
			if (val) {
				ccd_force_enable();
				ccprintf("Warning CCD will remain "
					 "enabled until it is "
					 "explicitly disabled.\n");
			} else {
				clear_keepalive();
			}
		} else if (argc == 2) {
			if (val) {
				rdd_attached();
			} else {
				if (keep_ccd_enabled)
					clear_keepalive();

				rdd_detached();
			}
		} else
			return EC_ERROR_PARAM1;
	}

	ccprintf("CCD:%14s\nAP UART:  %s\nEC UART:  %s\n",
		keep_ccd_enabled ? "forced enable" :
		ccd_is_enabled() ? " enabled" : "disabled",
		uartn_enabled(UART_AP) ? " enabled" : "disabled",
		uartn_enabled(UART_EC) ? " enabled" : "disabled");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ccd, command_ccd,
			"[uart|i2c|keepalive] [<BOOLEAN>]",
			"Get/set the case closed debug state");

