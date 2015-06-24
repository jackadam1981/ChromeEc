/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB mux driver */

#ifndef __CROS_EC_USB_MUX_H
#define __CROS_EC_USB_MUX_H

#include "usb_pd.h"

/* Muxing for the USB type C */
enum typec_mux {
	TYPEC_MUX_UNKNOWN = -1, /* Unknown / invalid config */
	TYPEC_MUX_NONE,         /* Open switch */
	TYPEC_MUX_USB,          /* USB only */
	TYPEC_MUX_DP,           /* DP only */
	TYPEC_MUX_DOCK,         /* Both USB and DP */
	TYPEC_MUX_CONFIG_COUNT, /* Number of distinct, valid configs */
};

/* Define the state of a USB mux */
struct usb_mux_state {
	enum typec_mux mux_mode;
	int polarity_swapped;
};

struct usb_mux_driver {
	int (*init)(int port_addr);
	int (*get)(int port_addr, struct usb_mux_state *mux_state);
	int (*set)(int port_addr, struct usb_mux_state *mux_state);
	int (*flip)(int port_addr);
};

struct usb_mux {
	const int port_addr;
	const struct usb_mux_driver *driver;
};

/* Supported USB mux drivers */
extern const struct usb_mux_driver pi3usb30532_usb_mux_driver;

/* USB muxes present in system, ordered by port#, defied at board-level */
extern struct usb_mux usb_muxes[];

/**
 * Initialize USB mux to its default state.
 *
 * @param port  Port number.
 */
void usb_mux_init(int port);

/**
 * Configure superspeed muxes on type-C port.
 *
 * @param port port number.
 * @param mux_mode mux selected function.
 * @param usb_config usb2.0 selected function.
 * @param polarity plug polarity (0=CC1, 1=CC2).
 */
void usb_mux_set(int port, enum typec_mux mux_mode,
		 enum usb_switch usb_config, int polarity);

/**
 * Query superspeed mux status on type-C port.
 *
 * @param port port number.
 * @param dp_str pointer to the DP string to return.
 * @param usb_str pointer to the USB string to return.
 * @return Non-zero if superspeed connection is enabled; otherwise, zero.
 */
int usb_mux_get(int port, const char **dp_str, const char **usb_str);

/**
 * Flip the superspeed muxes on type-C port.
 *
 * This is used for factory test automation. Note that this function should
 * only flip the superspeed muxes and leave CC lines alone. Without further
 * changes, this function MUST ONLY be used for testing purpose, because
 * the protocol layer loses track of the superspeed polarity and DP/USB3.0
 * connection may break.
 *
 * @param port port number.
 */
void usb_mux_flip(int port);
#endif
