/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "demo.h"
#include "gpio.h"
#include "hooks.h"
#include "lcd.h"
#include "registers.h"
#include "spi.h"
#include "i2c.h"

static void joy_interrupt(enum gpio_signal signal)
{
	if (signal == GPIO_JOY_SEL)
		demo_joy_sel();
	else if (signal == GPIO_JOY_LEFT)
		demo_joy_left();
	else if (signal == GPIO_JOY_DOWN)
		demo_joy_down();
	else if (signal == GPIO_JOY_RIGHT)
		demo_joy_right();
	else if (signal == GPIO_JOY_UP)
		demo_joy_up();
}

/* Must come after other header files and GPIO interrupts*/
#include "gpio_list.h"

/******************************************************************************/

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	{ 0, 1, GPIO_LCD_SPI_CS_L },
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"vbus", I2C_PORT_MASTER, 100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"cc1",  I2C_PORT_MASTER, 100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"cc2",  I2C_PORT_MASTER, 100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

static void board_spi_enable(void)
{
	spi_enable(0, 0);

	/* Enable clocks to SPI1 module */
	STM32_RCC_APBENR2 |= STM32_RCC_SPI1;

	/* Reset SPI1 */
	STM32_RCC_APBRSTR2 |= STM32_RCC_SPI1;
	STM32_RCC_APBRSTR2 &= ~STM32_RCC_SPI1;

	gpio_config_module(MODULE_SPI_MASTER, 1);
	spi_enable(0, 1);
}

static void board_init(void)
{
	gpio_set_level(GPIO_ENCC1, 1);
	gpio_set_level(GPIO_ENCC2, 1);

	board_spi_enable();
	lcd_init();

	gpio_enable_interrupt(GPIO_JOY_SEL);
	gpio_enable_interrupt(GPIO_JOY_LEFT);
	gpio_enable_interrupt(GPIO_JOY_DOWN);
	gpio_enable_interrupt(GPIO_JOY_RIGHT);
	gpio_enable_interrupt(GPIO_JOY_UP);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
