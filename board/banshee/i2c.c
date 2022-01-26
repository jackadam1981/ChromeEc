/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "compile_time_macros.h"
#include "hooks.h"
#include "i2c.h"

#define BOARD_ID_FAST_PLUS_CAPABLE	2

/* I2C port map configuration */
const struct i2c_port_t i2c_ports[] = {
	{
		/* I2C0 */
		.name = "sensor",
		.port = I2C_PORT_SENSOR,
		.kbps = 400,
		.scl = GPIO_EC_I2C_SENSOR_SCL,
		.sda = GPIO_EC_I2C_SENSOR_SDA,
	},
	{
		/* I2C1 */
		.name = "tcpc0,1",
		.port = I2C_PORT_USB_C0_C1_TCPC,
		.kbps = 1000,
		.scl = GPIO_EC_I2C_USB_C0_C1_TCPC_SCL,
		.sda = GPIO_EC_I2C_USB_C0_C1_TCPC_SDA,
	},
	{
		/* I2C2 */
		.name = "ppc",
		.port = I2C_PORT_USB_PPC,
		.kbps = 1000,
		.scl = GPIO_EC_I2C_USB_PPC_BC_SCL,
		.sda = GPIO_EC_I2C_USB_PPC_BC_SDA,
	},
	{
		/* I2C3 */
		.name = "retimer0,1",
		.port = I2C_PORT_USB_C0_C1_MUX,
		.kbps = 1000,
		.scl = GPIO_EC_I2C_USB_C0_C1_RT_SCL,
		.sda = GPIO_EC_I2C_USB_C0_C1_RT_SDA,
	},
	{
		/* I2C4 */
		.name = "tcpc2, 3",
		.port = I2C_PORT_USB_C2_C3_TCPC,
		.kbps = 1000,
		.scl = GPIO_EC_I2C_USB_C2_C3_TCPC_SCL,
		.sda = GPIO_EC_I2C_USB_C2_C3_TCPC_SDA,
		.flags = I2C_PORT_FLAG_DYNAMIC_SPEED,
	},
	{
		/* I2C5 */
		.name = "battery",
		.port = I2C_PORT_BATTERY,
		.kbps = 100,
		.scl = GPIO_EC_I2C_BAT_SCL,
		.sda = GPIO_EC_I2C_BAT_SDA,
	},
	{
		/* I2C6 */
		.name = "retimer2,3",
		.port = I2C_PORT_USB_C2_C3_MUX,
		.kbps = 1000,
		.scl = GPIO_EC_I2C_USB_C2_C3_RT_SCL,
		.sda = GPIO_EC_I2C_USB_C2_C3_RT_SDA,
		.flags = I2C_PORT_FLAG_DYNAMIC_SPEED,
	},
	{
		/* I2C7 */
		.name = "eeprom",
		.port = I2C_PORT_EEPROM,
		.kbps = 400,
		.scl = GPIO_EC_I2C_MISC_SCL_R,
		.sda = GPIO_EC_I2C_MISC_SDA_R,
	},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/*
 * I2C controllers are initialized in main.c. This sets the speed much
 * later, but before I2C peripherals are initialized.
 */
static void set_board_legacy_i2c_speeds(void)
{
	if (get_board_id() >= BOARD_ID_FAST_PLUS_CAPABLE)
		return;

	ccprints("setting USB DB I2C buses to 400 kHz\n");

	i2c_set_freq(I2C_PORT_USB_C0_C1_TCPC, I2C_FREQ_400KHZ);
	i2c_set_freq(I2C_PORT_USB_C2_C3_TCPC, I2C_FREQ_400KHZ);
	i2c_set_freq(I2C_PORT_USB_PPC, I2C_FREQ_400KHZ);
}
DECLARE_HOOK(HOOK_INIT, set_board_legacy_i2c_speeds, HOOK_PRIO_INIT_I2C - 1);
