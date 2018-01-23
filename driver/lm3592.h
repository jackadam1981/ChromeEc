/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI LM3592 KBBL driver.
 */

#ifndef __CROS_EC_LM3592_H
#define __CROS_EC_LM3592_H

/* 8-bit I2C address */
#define LM3592_I2C_ADDR     0x6C

#define LM3592_REG_GP       0x10
#define LM3592_REG_BMAIN	0xA0
#define LM3592_REG_BSUB		0xB0
#define LM3592_REG_GPIO		0x80

/* Control keyboard backlight  */
int lm3592_enable_backlight_power(int enabled);

#endif /* __CROS_EC_LM3592_H */
