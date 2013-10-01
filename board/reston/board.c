/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Reston board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "task.h"
#include "usb.h"
#include "util.h"

/* GPIO signal list.  Must match order from enum gpio_signal. */
const struct gpio_info gpio_list[] = {
	{"AMP_WARN",  GPIO_C, (1<<0),  GPIO_ODR_HIGH, NULL},
	{"AMP_EN",    GPIO_C, (1<<4),  GPIO_OUT_LOW, NULL},

	{"FSR_COLA",  GPIO_C, (1<<1),  GPIO_INPUT | GPIO_PULL_UP, NULL},
	{"FSR_COLB",  GPIO_C, (1<<3),  GPIO_INPUT | GPIO_PULL_UP, NULL},
	{"FSR_COLC",  GPIO_A, (1<<0),  GPIO_INPUT | GPIO_PULL_UP, NULL},
	{"FSR_COLD",  GPIO_A, (1<<1),  GPIO_INPUT | GPIO_PULL_UP, NULL},
	{"FSR_COLE",  GPIO_C, (1<<2),  GPIO_INPUT | GPIO_PULL_UP, NULL},
	{"FSR_COLF",  GPIO_A, (1<<2),  GPIO_INPUT | GPIO_PULL_UP, NULL},
	{"FSR_COLG",  GPIO_A, (1<<3),  GPIO_INPUT | GPIO_PULL_UP, NULL},
	{"FSR_COLH",  GPIO_A, (1<<4),  GPIO_INPUT | GPIO_PULL_UP, NULL},
	{"FSR_COLI",  GPIO_A, (1<<6),  GPIO_INPUT | GPIO_PULL_UP, NULL},
	{"FSR_COLJ",  GPIO_A, (1<<7),  GPIO_INPUT | GPIO_PULL_UP, NULL},
	{"FSR_ROW1",  GPIO_E, (1<<7),  GPIO_ODR_HIGH, NULL},
	{"FSR_ROW2",  GPIO_E, (1<<8),  GPIO_ODR_HIGH, NULL},
	{"FSR_ROW3",  GPIO_B, (1<<1),  GPIO_ODR_HIGH, NULL},
	{"FSR_ROW4",  GPIO_E, (1<<10), GPIO_ODR_HIGH, NULL},
	{"FSR_ROW5",  GPIO_E, (1<<9),  GPIO_ODR_HIGH, NULL},
	{"FSR_ROW6",  GPIO_E, (1<<12), GPIO_ODR_HIGH, NULL},
	{"FSR_ROW7",  GPIO_E, (1<<11), GPIO_ODR_HIGH, NULL},
	{"FSR_ROW8",  GPIO_E, (1<<13), GPIO_ODR_HIGH, NULL},
	{"FSR_ROW9",  GPIO_E, (1<<14), GPIO_ODR_HIGH, NULL},
	{"FSR_ROW10", GPIO_E, (1<<15), GPIO_ODR_HIGH, NULL},

	{"HAP_ROW1",  GPIO_B, (1<<4),  GPIO_OUT_LOW, NULL},
	{"HAP_ROW2",  GPIO_B, (1<<3),  GPIO_OUT_LOW, NULL},
	{"HAP_ROW3",  GPIO_A, (1<<15), GPIO_OUT_LOW, NULL},
	{"HAP_ROW4",  GPIO_A, (1<<14), GPIO_OUT_LOW, NULL},
	{"HAP_ROW5",  GPIO_A, (1<<13), GPIO_OUT_LOW, NULL},
	{"HAP_ROW6",  GPIO_D, (1<<4),  GPIO_OUT_LOW, NULL},
	{"HAP_ROW7",  GPIO_D, (1<<3),  GPIO_OUT_LOW, NULL},
	{"HAP_ROW8",  GPIO_D, (1<<1),  GPIO_OUT_LOW, NULL},
	{"HAP_ROW9",  GPIO_C, (1<<12), GPIO_OUT_LOW, NULL},
	{"HAP_ROW10", GPIO_D, (1<<2),  GPIO_OUT_LOW, NULL},
	{"HAP_COLA",  GPIO_D, (1<<0),  GPIO_OUT_LOW, NULL},
	{"HAP_COLB",  GPIO_C, (1<<11), GPIO_OUT_LOW, NULL},
	{"HAP_COLC",  GPIO_H, (1<<2),  GPIO_OUT_LOW, NULL},
	{"HAP_COLD",  GPIO_A, (1<<8),  GPIO_OUT_LOW, NULL},
	{"HAP_COLE",  GPIO_C, (1<<10), GPIO_OUT_LOW, NULL},
	{"HAP_COLF",  GPIO_B, (1<<5),  GPIO_OUT_LOW, NULL},
	{"HAP_COLG",  GPIO_C, (1<<9),  GPIO_OUT_LOW, NULL},
	{"HAP_COLH",  GPIO_C, (1<<8),  GPIO_OUT_LOW, NULL},
	{"HAP_COLI",  GPIO_C, (1<<7),  GPIO_OUT_LOW, NULL},
	/* Test points */
	{"TP_PB8",    GPIO_B, (1<<8),  GPIO_INPUT, NULL},
	{"TP_PB9",    GPIO_B, (1<<9),  GPIO_INPUT, NULL},
	{"TP_PD5",    GPIO_D, (1<<5),  GPIO_INPUT, NULL},
	{"TP_PD6",    GPIO_D, (1<<6),  GPIO_INPUT, NULL},
	{"TP_PD7",    GPIO_D, (1<<7),  GPIO_INPUT, NULL},
	{"TP_PE1",    GPIO_E, (1<<1),  GPIO_INPUT, NULL},
	{"TP_PE2",    GPIO_E, (1<<2),  GPIO_INPUT, NULL},
	{"TP_PE3",    GPIO_E, (1<<3),  GPIO_INPUT, NULL},
	{"TP_PE4",    GPIO_E, (1<<4),  GPIO_INPUT, NULL},
	{"MCU_WK2",   GPIO_C, (1<<13), GPIO_INPUT, NULL},
	/* non-GPIO pins */
	{"I2C1_SCL",  GPIO_B, (1<<6),  GPIO_ODR_HIGH, NULL},
	{"I2C1_SDA",  GPIO_B, (1<<7),  GPIO_ODR_HIGH, NULL},
	{"DAC_OUT",   GPIO_A, (1<<5),  GPIO_INPUT, NULL},

	/* Unimplemented signals which we need to emulate for now */
	GPIO_SIGNAL_NOT_IMPLEMENTED("I2C2_SCL"),
	GPIO_SIGNAL_NOT_IMPLEMENTED("I2C2_SDA"),
	GPIO_SIGNAL_NOT_IMPLEMENTED("ENTERING_RW"),
	GPIO_SIGNAL_NOT_IMPLEMENTED("WP_L"),
};
BUILD_ASSERT(ARRAY_SIZE(gpio_list) == GPIO_COUNT);

/* Initialize board. */
static void board_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* Pins with alternate functions */
const struct gpio_alt_func gpio_alt_funcs[] = {
	/* UART1 on PA9/PA10 */
	{GPIO_A, 0x0600, GPIO_ALT_USART, MODULE_UART},
	/* USB DM/DP on PA11/PA12 */
	/* !!! PA11/PA12 should NOT be configured as Alternate Function USB !!!
	 * !!! to get working USB functionality !!! */
	/*{GPIO_A, 0x1800, GPIO_ALT_USB,   MODULE_USB},*/
	/* I2C1 on PB6/PB7 */
	{GPIO_B, 0x00c0, GPIO_ALT_I2C,   MODULE_I2C},
	/* OSC in/out on PH0/PH1 */
	{GPIO_H, 0x0000, GPIO_ALT_SYS,   MODULE_CHIPSET},
};
const int gpio_alt_funcs_count = ARRAY_SIZE(gpio_alt_funcs);

/* ADC channels */
const struct adc_t adc_channels[] = {
	[ADC_CH_FSR_COLA] = {"FSR_COLA", 3300, 4096, 0, STM32_AIN(11)},
	[ADC_CH_FSR_COLB] = {"FSR_COLB", 3300, 4096, 0, STM32_AIN(13)},
	[ADC_CH_FSR_COLC] = {"FSR_COLC", 3300, 4096, 0, STM32_AIN(0)},
	[ADC_CH_FSR_COLD] = {"FSR_COLD", 3300, 4096, 0, STM32_AIN(1)},
	[ADC_CH_FSR_COLE] = {"FSR_COLE", 3300, 4096, 0, STM32_AIN(12)},
	[ADC_CH_FSR_COLF] = {"FSR_COLF", 3300, 4096, 0, STM32_AIN(2)},
	[ADC_CH_FSR_COLG] = {"FSR_COLG", 3300, 4096, 0, STM32_AIN(3)},
	[ADC_CH_FSR_COLH] = {"FSR_COLH", 3300, 4096, 0, STM32_AIN(4)},
	[ADC_CH_FSR_COLI] = {"FSR_COLI", 3300, 4096, 0, STM32_AIN(6)},
	[ADC_CH_FSR_COLJ] = {"FSR_COLJ", 3300, 4096, 0, STM32_AIN(7)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"host", I2C_PORT_MASTER, 100},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

const void * const usb_strings[] = {
	[USB_STR_DESC] = usb_string_desc,
	[USB_STR_VENDOR] = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT] = USB_STRING_DESC("Reston"),
	[USB_STR_VERSION] = USB_STRING_DESC("v0.01"),
};
BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);
