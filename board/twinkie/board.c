/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Twinkie dongle configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "task.h"
#include "usb.h"
#include "util.h"

/* GPIO signal list.  Must match order from enum gpio_signal. */
const struct gpio_info gpio_list[] = {
	/* PD RX/TX */
	{"USB_CC1_PD",        GPIO_A, (1<<0),  GPIO_ANALOG, NULL},
	{"PD_REF1",           GPIO_A, (1<<1),  GPIO_ANALOG, NULL},
	{"PD_REF2",           GPIO_A, (1<<3),  GPIO_ANALOG, NULL},
	{"USB_CC2_PD",        GPIO_A, (1<<4),  GPIO_ANALOG, NULL},
	{"PD_CLK_OUT",        GPIO_B, (1<<9),  GPIO_OUT_LOW, NULL},
	{"PD_TX_EN",          GPIO_B, (1<<12), GPIO_OUT_LOW, NULL},
#if 0
	{"PD_CLK_IN",         GPIO_B, (1<<13), GPIO_OUT_LOW, NULL},
	{"PD_TX_DATA",        GPIO_B, (1<<14), GPIO_OUT_LOW, NULL},
#endif

	/* Alternate functions */
#if 0
	{"USB_DM",            GPIO_A, (1<<11), GPIO_ANALOG, NULL},
	{"USB_DP",            GPIO_A, (1<<12), GPIO_ANALOG, NULL},
	{"UART_TX",           GPIO_A, (1<<14), GPIO_OUT_LOW, NULL},
	{"UART_RX",           GPIO_A, (1<<15), GPIO_OUT_LOW, NULL},
#endif
	/* Unimplemented signals which we need to emulate for now */
	GPIO_SIGNAL_NOT_IMPLEMENTED("ENTERING_RW"),
	GPIO_SIGNAL_NOT_IMPLEMENTED("WP_L"),
};
BUILD_ASSERT(ARRAY_SIZE(gpio_list) == GPIO_COUNT);

/* Initialize board. */
void board_config_pre_init(void)
{
	/* 40 MHz pin speed on UART PA14/15 */
	STM32_GPIO_OSPEEDR(GPIO_A) |= 0xF0000000;
	/* 40 MHz pin speed on PA0 and PA4 */
	STM32_GPIO_OSPEEDR(GPIO_A) |= 0x303;
}

/* Pins with alternate functions */
const struct gpio_alt_func gpio_alt_funcs[] = {
	{GPIO_B, 0x6000, 0, MODULE_USB_PD},/* SPI2: MISO(PB14) SCK(PB13) */
	{GPIO_B, 0x0200, 2, MODULE_USB_PD},/* TIM17_CH1: PB9) */
	{GPIO_A, 0xC000, 1, MODULE_UART},  /* USART2: PA14/PA15 */
};
const int gpio_alt_funcs_count = ARRAY_SIZE(gpio_alt_funcs);

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* USB PD CC lines sensing. Converted to mV (3300mV/4096). */
	[ADC_CH_CC1_PD] = {"CC1_PD", 3300, 4096, 0, STM32_AIN(0)},
	[ADC_CH_CC2_PD] = {"CC2_PD", 3300, 4096, 0, STM32_AIN(4)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

const void * const usb_strings[] = {
	[USB_STR_DESC] = usb_string_desc,
	[USB_STR_VENDOR] = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT] = USB_STRING_DESC("Twinkie"),
	[USB_STR_VERSION] = USB_STRING_DESC("vXX.YYY"),
};
BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);
