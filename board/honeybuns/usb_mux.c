/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Honeybuns-custom USB mux driver. */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "usb_mux.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

void board_set_usb_switches(int port, enum usb_switch setting)
{
	/* Not implemented */
}

static int board_init_usb_mux(int port)
{
	return 0;
}

static int board_set_usb_mux(int port, struct usb_mux_state *mux_state)
{
	enum typec_mux mux = mux_state->mux_mode;

	if (mux == TYPEC_MUX_NONE) {
		/* put the mux in the high impedance state */
		gpio_set_level(GPIO_SS_MUX_OE_L, 1);
		return 0;
	}

	if ((mux == TYPEC_MUX_DOCK) || (mux == TYPEC_MUX_USB)) {
		/* Low selects USB Dock */
		gpio_set_level(GPIO_SS_MUX_SEL, 0);
	} else if (mux == TYPEC_MUX_DP) {
		/* high selects display port */
		gpio_set_level(GPIO_SS_MUX_SEL, 1);
	}

	/* clear OE line to make mux active */
	gpio_set_level(GPIO_SS_MUX_OE_L, 0);

	return 0;
}

static int board_get_usb_mux(int port, struct usb_mux_state *mux_state)
{
	int oe_disabled = gpio_get_level(GPIO_SS_MUX_OE_L);
	int dp_4lanes = gpio_get_level(GPIO_SS_MUX_SEL);
	mux_state->polarity_swapped = 0;

	if (oe_disabled)
		mux_state->mux_mode = TYPEC_MUX_NONE;
	else if (dp_4lanes)
		mux_state->mux_mode = TYPEC_MUX_DP;
	else
		mux_state->mux_mode = TYPEC_MUX_DOCK;

	return 0;
}

static int board_flip_usb_mux(int port)
{
	/* Not implemented */
	return 0;
}

const struct usb_mux_driver board_custom_usb_mux_driver = {
	.init = board_init_usb_mux,
	.get = board_get_usb_mux,
	.set = board_set_usb_mux,
	.flip = board_flip_usb_mux,
};

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_COUNT] = {
	{
		.port_addr = 0,
		.driver    = &board_custom_usb_mux_driver,
	},
};
