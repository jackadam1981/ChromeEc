/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* rei board configuration */

#include "common.h"
#include "gpio.h"
#include "i2c.h"
#include "registers.h"
#include "util.h"

#include "gpio_list.h"

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"sensors", 3, 400, GPIO_I2C5_SCL, GPIO_I2C5_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);
