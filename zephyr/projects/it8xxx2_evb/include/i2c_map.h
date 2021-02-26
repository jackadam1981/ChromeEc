/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_CHROME_I2C_MAP_H
#define __ZEPHYR_CHROME_I2C_MAP_H

#include <devicetree.h>

#include "config.h"

/* We need registers.h to get the chip specific defines for now */
#include "i2c/i2c.h"

#define I2C_PORT_BATTERY	NAMED_I2C(battery)
#define IT83XX_I2C_CH_A		NAMED_I2C(evb_1)
#define IT83XX_I2C_CH_B		NAMED_I2C(evb_2)
#define IT83XX_I2C_CH_E		NAMED_I2C(opt_4)


#endif /* __ZEPHYR_CHROME_I2C_MAP_H */
