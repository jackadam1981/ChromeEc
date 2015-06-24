/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Ryu-custom USB mux driver. */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "usb_mux.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static int board_init_usb_mux(int port)
{
	return 0;
}

static int board_set_usb_mux(int port, struct usb_mux_state *mux_state)
{
	enum typec_mux mux = mux_state->mux_mode;
	int polarity = mux_state->polarity_swapped;

	/* reset everything */
	gpio_set_level(GPIO_USBC_MUX_CONF0, 0);
	gpio_set_level(GPIO_USBC_MUX_CONF1, 0);
	gpio_set_level(GPIO_USBC_MUX_CONF2, 0);

	if (mux == TYPEC_MUX_NONE)
		/* everything is already disabled, we can return */
		return 0;

	gpio_set_level(GPIO_USBC_MUX_CONF0, polarity);

	if (mux == TYPEC_MUX_USB || mux == TYPEC_MUX_DOCK)
		/* USB 3.0 uses 2 superspeed lanes */
		gpio_set_level(GPIO_USBC_MUX_CONF2, 1);

	if (mux == TYPEC_MUX_DP || mux == TYPEC_MUX_DOCK)
		/* DP uses available superspeed lanes (x2 or x4) */
		gpio_set_level(GPIO_USBC_MUX_CONF1, 1);

	return 0;
}

/* TODO(crosbug.com/p/38333) remove me */
#define GPIO_USBC_SS1_USB_MODE_L GPIO_USBC_MUX_CONF0
#define GPIO_USBC_SS2_USB_MODE_L GPIO_USBC_MUX_CONF1
#define GPIO_USBC_SS_EN_L GPIO_USBC_MUX_CONF2

static int p4_board_set_usb_mux(int port, struct usb_mux_state *mux_state)
{
	enum typec_mux mux = mux_state->mux_mode;
	int polarity = mux_state->polarity_swapped;

	/* reset everything */
	gpio_set_level(GPIO_USBC_SS_EN_L, 1);
	gpio_set_level(GPIO_USBC_DP_MODE_L, 1);
	gpio_set_level(GPIO_USBC_DP_POLARITY, 1);
	gpio_set_level(GPIO_USBC_SS1_USB_MODE_L, 1);
	gpio_set_level(GPIO_USBC_SS2_USB_MODE_L, 1);

	if (mux == TYPEC_MUX_NONE)
		/* everything is already disabled, we can return */
		return 0;

	if (mux == TYPEC_MUX_USB || mux == TYPEC_MUX_DOCK) {
		/* USB 3.0 uses 2 superspeed lanes */
		gpio_set_level(polarity ? GPIO_USBC_SS2_USB_MODE_L :
					  GPIO_USBC_SS1_USB_MODE_L, 0);
	}

	if (mux == TYPEC_MUX_DP || mux == TYPEC_MUX_DOCK) {
		/* DP uses available superspeed lanes (x2 or x4) */
		gpio_set_level(GPIO_USBC_DP_POLARITY, polarity);
		gpio_set_level(GPIO_USBC_DP_MODE_L, 0);
	}
	/* switch on superspeed lanes */
	gpio_set_level(GPIO_USBC_SS_EN_L, 0);

	return 0;
}

static int board_get_usb_mux(int port, struct usb_mux_state *mux_state)
{
	int has_usb, has_dp;

	mux_state->polarity_swapped = gpio_get_level(GPIO_USBC_MUX_CONF0);
	has_usb = gpio_get_level(GPIO_USBC_MUX_CONF2);
	has_dp = gpio_get_level(GPIO_USBC_MUX_CONF1);

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

static int p4_board_get_usb_mux(int port, struct usb_mux_state *mux_state)
{
	int has_usb = !gpio_get_level(GPIO_USBC_SS1_USB_MODE_L) ||
		      !gpio_get_level(GPIO_USBC_SS2_USB_MODE_L);
	int has_dp = !gpio_get_level(GPIO_USBC_DP_MODE_L);

	mux_state->polarity_swapped = gpio_get_level(GPIO_USBC_DP_POLARITY);

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
	struct usb_mux_state mux_state;

	board_get_usb_mux(port, &mux_state);
	mux_state.polarity_swapped = !mux_state.polarity_swapped;
	board_set_usb_mux(port, &mux_state);
	return 0;
}

const struct usb_mux_driver p4_board_custom_usb_mux_driver = {
	.init = board_init_usb_mux,
	.get = p4_board_get_usb_mux,
	.set = p4_board_set_usb_mux,
	.flip = board_flip_usb_mux,
};

const struct usb_mux_driver p5_board_custom_usb_mux_driver = {
	.init = board_init_usb_mux,
	.get = board_get_usb_mux,
	.set = board_set_usb_mux,
	.flip = board_flip_usb_mux,
};

