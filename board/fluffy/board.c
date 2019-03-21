/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fluffy configuration */

#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "usb_descriptor.h"
#include "registers.h"
#include "util.h"

#include "gpio_list.h"

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */

const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("Fluffy"),
	[USB_STR_SERIALNO]     = USB_STRING_DESC("1234-a"),
	[USB_STR_VERSION]      = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("Fluffy Shell"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

enum usb_mux {
	USB_MUX0 = 0,
	USB_MUX1,
	USB_MUX2,
	USB_MUX_COUNT,
};

static void set_mux(enum usb_mux mux, uint8_t val)
{
	enum gpio_signal c0;
	enum gpio_signal c1;
	enum gpio_signal c2;

	switch (mux) {
	case USB_MUX0:
		c0 = GPIO_USB_MUX0_C0;
		c1 = GPIO_USB_MUX0_C1;
		c2 = GPIO_USB_MUX0_C2;
		break;

	case USB_MUX1:
		c0 = GPIO_USB_MUX1_C0;
		c1 = GPIO_USB_MUX1_C1;
		c2 = GPIO_USB_MUX1_C2;
		break;

	case USB_MUX2:
		c0 = GPIO_USB_MUX2_C0;
		c1 = GPIO_USB_MUX2_C1;
		c2 = GPIO_USB_MUX2_C2;
		break;

	default:
		break;
	}

	val &= 0x7;

	gpio_set_level(c0, val & 0x1);
	gpio_set_level(c1, !!(val & 0x2));
	gpio_set_level(c2, !!(val & 0x4));
}

static enum gpio_signal enabled_port = GPIO_EN_C0;

/* This function assumes only 1 port works at a time. */
static int command_portctl(int argc, char **argv)
{
	int port;
	int enable;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	port = atoi(argv[1]);
	if ((port < 0) || (port > 19) || !parse_bool(argv[2], &enable))
		return EC_ERROR_INVAL;

	gpio_set_level(GPIO_EN_USB_MUX0, 0);
	gpio_set_level(GPIO_EN_USB_MUX1, 0);
	gpio_set_level(GPIO_EN_USB_MUX2, 0);

	/*
	 * For each port, we must configure the USB 2.0 muxes and make sure that
	 * the power enables are configured as desired.
	 */

	gpio_set_level(enabled_port, 0);

	if (enable) {
		enabled_port = GPIO_EN_C0 + port;
		gpio_set_level(enabled_port, 1);

		if (port > 15) {
			set_mux(USB_MUX2, port - 14);
			gpio_set_level(GPIO_EN_USB_MUX2, 1);
		} else if (port > 7) {
			set_mux(USB_MUX2, 1);
			set_mux(USB_MUX1, port - 8);
			gpio_set_level(GPIO_EN_USB_MUX2, 1);
			gpio_set_level(GPIO_EN_USB_MUX1, 1);
		} else {
			set_mux(USB_MUX2, 0);
			set_mux(USB_MUX0, port);
			gpio_set_level(GPIO_EN_USB_MUX2, 1);
			gpio_set_level(GPIO_EN_USB_MUX0, 1);
		}
	}

	gpio_set_level(GPIO_EN_PP3300_USBMUX, !!enable);
	gpio_set_level(GPIO_EN_PP5000_GATE_DR, !!enable);


	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(portctl, command_portctl,
			"<port# 0-19> <enable/disable>",
			"enable or disable a port");
