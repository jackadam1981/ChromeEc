/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "compile_time_macros.h"

#include "i2c.h"

/* I2C port map configuration */
const struct i2c_port_t i2c_ports[] = {
	{
		/* I2C0 */
		.name = "tcpc0,2",
		.port = I2C_PORT_USB_C0_C2_TCPC,
		.kbps = 1000,
		.scl = GPIO_EC_I2C_USB_C0_C2_TCPC_SCL,
		.sda = GPIO_EC_I2C_USB_C0_C2_TCPC_SDA,
	},
	{
		/* I2C1 */
		.name = "ppc0,2",
		.port = I2C_PORT_USB_C0_C2_PPC,
		.kbps = 1000,
		.scl = GPIO_EC_I2C_USB_C0_C2_PPC_BC_SCL,
		.sda = GPIO_EC_I2C_USB_C0_C2_PPC_BC_SDA,
	},
	{
		/* I2C2 */
		.name = "retimer0,2",
		.port = I2C_PORT_USB_C0_C2_MUX,
		.kbps = 1000,
		.scl = GPIO_EC_I2C_USB_C0_C2_RT_SCL,
		.sda = GPIO_EC_I2C_USB_C0_C2_RT_SDA,
	},
	{
		/* I2C3 C1 TCPC */
		.name = "tcpc1",
		.port = I2C_PORT_USB_C1_TCPC,
		.kbps = 400,
		.scl = GPIO_EC_I2C_USB_C1_SCL,
		.sda = GPIO_EC_I2C_USB_C1_SDA,
	},
	{
		/* I2C4 */
		.name = "wireless_charger",
		.port = I2C_PORT_QI,
		.kbps = 400,
		.scl = GPIO_EC_I2C_QI_SCL,
		.sda = GPIO_EC_I2C_QI_SDA,
	},
	{
		/* I2C5 */
		.name = "eeprom",
		.port = I2C_PORT_EEPROM,
		.kbps = 400,
		.scl = GPIO_EC_I2C_MISC_SCL,
		.sda = GPIO_EC_I2C_MISC_SDA,
	},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);
