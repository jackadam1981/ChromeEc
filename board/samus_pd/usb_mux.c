/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Samus PD-custom USB mux driver. */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "usb_mux.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

struct usb_port_mux {
	enum gpio_signal ss1_en_l;
	enum gpio_signal ss2_en_l;
	enum gpio_signal dp_mode_l;
	enum gpio_signal dp_polarity;
	enum gpio_signal ss1_dp_mode;
	enum gpio_signal ss2_dp_mode;
};

static const struct usb_port_mux mux_gpios[] = {
	{
		.ss1_en_l    = GPIO_USB_C0_SS1_EN_L,
		.ss2_en_l    = GPIO_USB_C0_SS2_EN_L,
		.dp_mode_l   = GPIO_USB_C0_DP_MODE_L,
		.dp_polarity = GPIO_USB_C0_DP_POLARITY,
		.ss1_dp_mode = GPIO_USB_C0_SS1_DP_MODE,
		.ss2_dp_mode = GPIO_USB_C0_SS2_DP_MODE,
	},
	{
		.ss1_en_l    = GPIO_USB_C1_SS1_EN_L,
		.ss2_en_l    = GPIO_USB_C1_SS2_EN_L,
		.dp_mode_l   = GPIO_USB_C1_DP_MODE_L,
		.dp_polarity = GPIO_USB_C1_DP_POLARITY,
		.ss1_dp_mode = GPIO_USB_C1_SS1_DP_MODE,
		.ss2_dp_mode = GPIO_USB_C1_SS2_DP_MODE,
	},
};
BUILD_ASSERT(ARRAY_SIZE(mux_gpios) == CONFIG_USB_PD_PORT_COUNT);


static int board_init_usb_mux(int port)
{
	return 0;
}

static int board_set_usb_mux(int port, struct usb_mux_state *mux_state)
{
	const struct usb_port_mux *usb_mux = mux_gpios + port;
	enum typec_mux mux = mux_state->mux_mode;
	int polarity = mux_state->polarity_swapped;

	/* reset everything */
	gpio_set_level(usb_mux->ss1_en_l, 1);
	gpio_set_level(usb_mux->ss2_en_l, 1);
	gpio_set_level(usb_mux->dp_mode_l, 1);
	gpio_set_level(usb_mux->dp_polarity, 1);
	gpio_set_level(usb_mux->ss1_dp_mode, 1);
	gpio_set_level(usb_mux->ss2_dp_mode, 1);

	if (mux == TYPEC_MUX_NONE)
		/* everything is already disabled, we can return */
		return 0;

	if (mux == TYPEC_MUX_USB || mux == TYPEC_MUX_DOCK) {
		/* USB 3.0 uses 2 superspeed lanes */
		gpio_set_level(polarity ? usb_mux->ss2_dp_mode :
					  usb_mux->ss1_dp_mode, 0);
	}

	if (mux == TYPEC_MUX_DP || mux == TYPEC_MUX_DOCK) {
		/* DP uses available superspeed lanes (x2 or x4) */
		gpio_set_level(usb_mux->dp_polarity, polarity);
		gpio_set_level(usb_mux->dp_mode_l, 0);
	}
	/* switch on superspeed lanes */
	gpio_set_level(usb_mux->ss1_en_l, 0);
	gpio_set_level(usb_mux->ss2_en_l, 0);

	return 0;
}

static int board_get_usb_mux(int port, struct usb_mux_state *mux_state)
{
	const struct usb_port_mux *usb_mux = mux_gpios + port;
	int has_usb, has_dp;

	mux_state->polarity_swapped = gpio_get_level(usb_mux->dp_polarity);
	has_usb = !gpio_get_level(usb_mux->ss1_dp_mode) ||
		  !gpio_get_level(usb_mux->ss2_dp_mode);
	has_dp = !gpio_get_level(usb_mux->dp_mode_l);

	if (has_usb && has_dp)
		mux_state->mux_mode = TYPEC_MUX_DOCK;
	else if (has_usb)
		mux_state->mux_mode = TYPEC_MUX_USB;
	else if (has_dp)
		mux_state->mux_mode = TYPEC_MUX_DP;
	else
		mux_state->mux_mode = TYPEC_MUX_NONE;

	return 0;
}

static int board_flip_usb_mux(int port)
{
	const struct usb_port_mux *usb_mux = mux_gpios + port;
	int usb_polarity;

	/* Flip DP polarity */
	gpio_set_level(usb_mux->dp_polarity,
		       !gpio_get_level(usb_mux->dp_polarity));

	/* Flip USB polarity if enabled */
	if (gpio_get_level(usb_mux->ss1_dp_mode) &&
	    gpio_get_level(usb_mux->ss2_dp_mode))
		return 0;
	usb_polarity = gpio_get_level(usb_mux->ss1_dp_mode);

	/*
	 * Disable both sides first so that we don't enable both at the
	 * same time accidentally.
	 */
	gpio_set_level(usb_mux->ss1_dp_mode, 1);
	gpio_set_level(usb_mux->ss2_dp_mode, 1);

	gpio_set_level(usb_mux->ss1_dp_mode, !usb_polarity);
	gpio_set_level(usb_mux->ss2_dp_mode, usb_polarity);

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
	{
		.port_addr = 0,
		.driver    = &board_custom_usb_mux_driver,
	},
};
