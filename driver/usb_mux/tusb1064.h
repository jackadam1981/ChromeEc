/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI TUSB1064
 * USB Type-C Retiming Switch for USB Device / DisplayPort Sink
 */

#ifndef __CROS_EC_PS8740_H
#define __CROS_EC_PS8740_H

#include "usb_mux.h"

#define TUSB1064_I2C_ADDR0_FLAG    0x44
#define TUSB1064_I2C_ADDR1_FLAG    0x47
#define TUSB1064_I2C_ADDR2_FLAG    0x0C
#define TUSB1064_I2C_ADDR3_FLAG    0x0F

/* Mode register for setting mux */
#define TUSB1064_REG_MODE         0x0A

#define TUSB1064_MODE_FLIPSEL     BIT(2)
#define TUSB1064_MODE_ALT_DP_EN   BIT(1)
#define TUSB1064_MODE_USB_EN      BIT(0)

#define TUSB1064_REG_DP_CONTROL   0x13
#define TUSB1064_REG_DP_AUX_SNOOP_DIS   BIT(7)

int tusb1064_write(const struct usb_mux *me, uint8_t reg, uint8_t val);
int tusb1064_read(const struct usb_mux *me, uint8_t reg, int *val);

#endif /* __CROS_EC_PS8740_H */
