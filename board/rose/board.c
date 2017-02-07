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
#include "usb_descriptor.h"
#include "util.h"
#include "usb_dwc_hw.h"
#include "usb_dwc_console.h"

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"i2c1", I2C_PORT_0, 800, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* SPI ports */
const struct spi_device_t spi_devices[] = {
	{ 0, 2, GPIO_SPI1_NSS },
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

#define GPIO_SET_HS(bank, number)	\
	(STM32_GPIO_OSPEEDR(GPIO_##bank) |= (0x3 << ((number) * 2)))

void board_config_post_gpio_init(void)
{
	/* Set USB GPIO to high speed */
	GPIO_SET_HS(A, 11);
	GPIO_SET_HS(A, 12);

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

	/*
	 * TODO(crosbug.com/p/31390): Reset the SPI Peripheral to clear any
	 * existing weird states.
	 */
	STM32_RCC_APB2RSTR |= (1 << 12);
	STM32_RCC_APB2RSTR &= ~(1 << 12);
	/* Enable clocks to SPI1 */
	STM32_RCC_APB2ENR |= STM32_RCC_PB2_SPI1;
	/* Delay 1 APB clock cycle after the clock is enabled */
	clock_wait_bus_cycles(BUS_APB, 1);
	/* Set SPI pins to alternate function */
	gpio_config_module(MODULE_SPI_MASTER, 1);
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


/* Debugging feature and command, remove before merge */

const void *const usb_strings[] = {
	[USB_STR_DESC]		= usb_string_desc,
	[USB_STR_VENDOR]	= USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]	= USB_STRING_DESC("HSP"),
	[USB_STR_SERIALNO]	= USB_STRING_DESC("1234-a"),
	[USB_STR_VERSION]	= USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME]	= USB_STRING_DESC("Shell"),
};
BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

struct dwc_usb usb_ctl = {
	.ep = {
		&ep0_ctl,
		&ep_console_ctl,
	},
	.speed = USB_SPEED_FS,
	.phy_type = USB_PHY_INTERNAL,
	.dma_en = 0,
	.irq = STM32_IRQ_OTG_FS,
};

int command_nvic(int argc, char *argv[])
{
	int i;

	ccputs("NVIC enabled bits :\n");

	for (i = 0; i < CONFIG_IRQ_COUNT; i++) {
		if (CPU_NVIC_EN(i / 32) & (1 << (i % 32)))
			ccprintf("%d ", i);
	}
	ccputs("\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND
	(nvic, command_nvic,
	 NULL, "Read Cortex-m NVIC enable bits");


