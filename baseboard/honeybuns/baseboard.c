/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Honeybuns family-specific configuration */
#include "gpio.h"
#include "i2c.h"

/******************************************************************************/
/* I2C port map configuration */
const struct i2c_port_t i2c_ports[] = {
	{"usbc",   I2C_PORT_USBC,   400, GPIO_EC_I2C1_SCL, GPIO_EC_I2C1_SDA},
	{"usb_mst",  I2C_PORT_MST,  400, GPIO_EC_I2C2_SCL, GPIO_EC_I2C2_SDA},
	{"eeprom",  I2C_PORT_EEPROM,  400, GPIO_EC_I2C3_SCL, GPIO_EC_I2C3_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

