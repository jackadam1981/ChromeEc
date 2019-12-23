/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * PS8802 retimer.
 */

#ifndef __CROS_EC_USB_RETIMER_PS8802_H
#define __CROS_EC_USB_RETIMER_PS8802_H

#define PS8802_I2C_ADDR_FLAGS	0x0a

#define PS8802_REG_MODE		0x06
#define PS8802_MODE_DP_REG_CONTROL	BIT(7)
#define PS8802_MODE_DP_ENABLE		BIT(6)
#define PS8802_MODE_USB_REG_CONTROL	BIT(5)
#define PS8802_MODE_USB_ENABLE		BIT(4)
#define PS8802_MODE_FLIP_REG_CONTROL	BIT(3)
#define PS8802_MODE_FLIP_ENABLE		BIT(2)
#define PS8802_MODE_IN_HPD_REG_CONTROL	BIT(1)
#define PS8802_MODE_IN_HPD_ENABLE	BIT(0)

extern const struct usb_retimer_driver ps8802_usb_retimer;

#endif /* __CROS_EC_USB_RETIMER_PS8802_H */
