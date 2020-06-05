/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Parade PS8822
 * USB Type-C Retiming Switch for USB Device / DisplayPort Sink
 */

#ifndef __CROS_EC_PS8740_H
#define __CROS_EC_PS8740_H

#include "usb_mux.h"

#define PS8822_I2C_ADDR0_FLAG    0x10
#define PS8822_I2C_ADDR1_FLAG    0x18
#define PS8822_I2C_ADDR2_FLAG    0x58
#define PS8822_I2C_ADDR3_FLAG    0x60

/* Mode register for setting mux */
#define PS8822_REG_MODE         0x01
#define PS8822_MODE_ALT_DP_EN   BIT(7)
#define PS8822_MODE_USB_EN      BIT(6)
#define PS8822_MODE_FLIP        BIT(5)
#define PS8822_MODE_PIN_E       BIT(4)

#define PS8822_REG_CONFIG       0x02
#define PS8822_CONFIG_HPD_IN_DIS BIT(7)
#define PS8822_CONFIG_DP_PLUG    BIT(6)



int ps8822_write(const struct usb_mux *me, uint8_t reg, uint8_t val);
int ps8822_read(const struct usb_mux *me, uint8_t reg, int *val);

#endif /* __CROS_EC_PS8740_H */
