/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Rose board configuration */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "dma.h"
#include "ec_version.h"
#include "gpio.h"
#include "gpio_list.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "spi.h"
#include "stm32-dma.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"i2c1", 0, 100, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* SPI ports */
const struct spi_device_t spi_devices[] = {
	{ 0, 3, GPIO_SPI1_NSS },
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

#define GPIO_SET_HS(bank, number)	\
	(STM32_GPIO_OSPEEDR(GPIO_##bank) |= (0x3 << ((number) * 2)))

void board_config_post_gpio_init(void)
{
	/* Set I2C GPIO to HS */
	GPIO_SET_HS(B,  6);
	GPIO_SET_HS(B,  7);

	/* Set SPI GPIO to HS */
	GPIO_SET_HS(A,  5);
	GPIO_SET_HS(A,  7);
}

static void board_init(void)
{
	/* Enable GPIO interrupt */
	gpio_enable_interrupt(GPIO_SENSOR_INT);
	gpio_enable_interrupt(GPIO_BUTTON_INT);

	/* Reset SPI1 */
	spi_enable(0, 0);
	STM32_RCC_APB2RSTR |= STM32_RCC_PB2_SPI1;
	STM32_RCC_APB2RSTR &= ~STM32_RCC_PB2_SPI1;
	/* Enable clocks to SPI1 */
	STM32_RCC_APB2ENR |= STM32_RCC_PB2_SPI1;
	/* Delay 1 APB clock cycle after the clock is enabled */
	clock_wait_bus_cycles(BUS_APB, 1);
	/* Set SPI pins to alternate function */
	gpio_config_module(MODULE_SPI_MASTER, 1);
	spi_enable(0, 1);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

void button_debounce(void)
{
	ccputs("button\n");
}
DECLARE_DEFERRED(button_debounce);

void button_event(enum gpio_signal signal)
{
	hook_call_deferred(&button_debounce_data, -1);
	hook_call_deferred(&button_debounce_data, 100 * MSEC);
}

void sensor_event(enum gpio_signal signal)
{
}

void board_i2c_process(int read, uint8_t addr, int len, char *buffer,
		       void (*send_response)(int len))
{
	int i;

	ccprintf("i2c: r:%d a:%02x l:%d buf:",
		 read, addr, len);
	for (i = 0; i < len; i++)
		ccprintf("%02x ", buffer[i]);
	if (read)
		send_response(len);
}

/* Debugging feature and command, remove before merge */

int command_tid(int argc, char *argv[])
{
	uint8_t cmd[3] = { 0xb4, 0x00, 0x04 };
	uint8_t ret[7];
	const struct spi_device_t *dev = &spi_devices[0];
	int rv, i;

	rv = spi_transaction(dev, cmd, ARRAY_SIZE(cmd), ret, ARRAY_SIZE(ret));
	if (rv)
		ccprintf("spi_transaction() returns %d\n", rv);

	ccputs("tid(b4 00 04):");
	for (i = 0; i < ARRAY_SIZE(ret); i++)
		ccprintf(" %02x", ret[i]);
	ccputs("\n");

	return 0;
}
DECLARE_CONSOLE_COMMAND
	(tid, command_tid,
	 NULL, "Read TID from SPI sensor");
