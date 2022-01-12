/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_PD_RETIMER_ANX7443_H
#define __CROS_EC_USB_PD_RETIMER_ANX7443_H

#include "usb_mux.h"

#define I2C_ADDR(x)				((x) >> 1)

#define I2C0_TOP_SLAVE				0x20
#define INSERT_CR_PATTERN_0			0x4B

#define DP_RX_REG0				0x72
#define HIGHEST_DRV_STRENTH			(BIT(4) | BIT(5))
#define RX_GAIN					BIT(3)
#define LFE_EN					BIT(2)
#define CTLE_CTRL				BIT(1)

#define DP_RX_REG3				0x75
#define CTLE_DRV_STRENTH			BIT(4) | BIT(5) | BIT(6) | BIT(7)
#define CTLE_OFF_CANCELLING_EN			BIT(1)

#define CONFIG_MODE				0xF8
#define CONFIG_REG_EN				BIT(4)
#define FLIP_EN					BIT(2)
#define DP_EN					BIT(1)
#define USB_EN					BIT(0)

#define I2C0_USB_SLAVE				0x52
#define FLIP_CTRL				0xA4
#define USB_AUX_FLIP_EN				BIT(5)

#define I2C0_DP_SLAVE				0x8E
#define POWER_DOWN				0x82

extern const struct usb_mux_driver anx7443_usbc_retimer_driver;

#endif /* __CROS_EC_USB_PD_RETIMER_ANX7443_H */
