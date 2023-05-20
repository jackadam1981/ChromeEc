/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ANX7452: Active redriver
 *
 * Public functions, definitions, and structures.
 */

#ifndef __CROS_EC_USB_RETIMER_ANX7452_PUBLIC_H
#define __CROS_EC_USB_RETIMER_ANX7452_PUBLIC_H

#include "usb_mux.h"

extern const struct usb_mux_driver anx7452_usb_retimer_driver;

/* Retimer driver hardware specific controls */
struct anx7452_control {
	/* USB enable */
	const enum gpio_signal usb_enable_gpio;
	/* DP enable */
	const enum gpio_signal dp_enable_gpio;
};
extern const struct anx7452_control anx7452_controls[];

/*
 * ANX7452 uses a multiple registers. One of the registers related to USB
 * functionality has address conflict in some boards. This function helps in
 * determining whether to handle that case in driver based on per board
 * definition.
 */
bool board_anx7452_is_usb_addr_conflict_present(const struct usb_mux *me);

#endif /* __CROS_EC_USB_RETIMER_ANX7452_PUBLIC_H */
