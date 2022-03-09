/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ANX7483: Active redriver with linear equilzation
 */

#ifndef __CROS_EC_USB_MUX_ANX7483_H
#define __CROS_EC_USB_MUX_ANX7483_H

#include "usb_mux.h"

/* I2C interface addresses */
#define ANX7483_I2C_ADDR0_FLAGS		0x3E
#define ANX7483_I2C_ADDR1_FLAGS		0x38
#define ANX7483_I2C_ADDR2_FLAGS		0x40
#define ANX7483_I2C_ADDR3_FLAGS		0x44

#define ANX7483_ANALOG_STATUS_CTRL	0x07
#define ANX7483_CTRL_REG_BYPASS_EN	BIT(5)
#define ANX7483_CTRL_REG_EN		BIT(4)
#define ANX7483_CTRL_FLIP_EN		BIT(2)
#define ANX7483_CTRL_DP_EN		BIT(1)
#define ANX7483_CTRL_USB_EN		BIT(0)

extern const struct usb_mux_driver anx7483_usb_retimer_driver;
#endif /* __CROS_EC_USB_MUX_ANX7483_H */
