/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Glower board-specific configuration */

#include "gpio.h"
#include "i2c.h"
#include "registers.h"
#include "util.h"

#define GPIO_KB_INPUT GPIO_INPUT
#define GPIO_KB_OUTPUT (GPIO_ODR_HIGH | GPIO_PULL_UP)

#include "gpio_list.h"

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"port1", 1, 100},
	{"port2", 2, 100},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);
