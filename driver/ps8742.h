/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Parade PS8742 USB Type-C SS/DP demax.
 */

#ifndef __CROS_EC_PS8742_H
#define __CROS_EC_PS8742_H

#define PS8742_I2C_ADDR0         (0x10 << 1)
#define PS8742_I2C_ADDR1         (0x11 << 1)
#define PS8742_I2C_ADDR2         (0x19 << 1)
#define PS8742_I2C_ADDR3         (0x1a << 1)

#define PS8742_REG_MODE          0x00

#define PS8742_MODE_PIN_E_OR_C   (1 << 7)
#define PS8742_MODE_DP           (1 << 6)
#define PS8742_MODE_USB          (1 << 5)
#define PS8742_MODE_FLIP         (1 << 4)

#endif /* __CROS_EC_PS8742_H */
