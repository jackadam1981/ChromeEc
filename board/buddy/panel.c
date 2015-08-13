/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* EC for Buddy panel control */

#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "timer.h"

#define I2C_PORT_CONVERT 0
#define I2C_ADDR_CONVERT 0x62

static void panel_converter_setting(void)
{
	if (gpio_get_level(CONFIG_BACKLIGHT_REQ_GPIO)) {
		if (gpio_get_level(GPIO_PANEL_ID1)) {
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x00,
			0xB1);
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x01,
			0x43);
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x02,
			0x7B);
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x03,
			0x00);
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x04,
			0x00);
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x05,
			0xB7);
		} else {
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x00,
			0xB1);
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x01,
			0x43);
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x02,
			0x73);
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x03,
			0x00);
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x04,
			0x00);
			i2c_write8(I2C_PORT_CONVERT, I2C_ADDR_CONVERT, 0x05,
			0xC7);
		}
	}
}
DECLARE_DEFERRED(panel_converter_setting);

void backlight_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&panel_converter_setting, 200 * MSEC);
}

static void convert_init(void)
{
#ifdef CONFIG_BACKLIGHT_REQ_GPIO
	gpio_enable_interrupt(CONFIG_BACKLIGHT_REQ_GPIO);
#endif
}
DECLARE_HOOK(HOOK_INIT, convert_init, HOOK_PRIO_DEFAULT);
