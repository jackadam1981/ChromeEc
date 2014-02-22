/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* STM32L-discovery board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "util.h"

void button_event(enum gpio_signal signal)
{
}

/* GPIO signal list.  Must match order from enum gpio_signal. */
const struct gpio_info gpio_list[] = {
	/* Inputs with interrupt handlers are first for efficiency */
	{"USER_BUTTON", GPIO_A, (1<<0),  GPIO_INT_BOTH, button_event},
	{"DAC_OUT1", GPIO_A, (1<<4),  GPIO_ANALOG, NULL},
	{"PD_RX", GPIO_B, (1<<4),  GPIO_ANALOG, NULL},
	/* Outputs */
	{"PD_TX", GPIO_A, (1<<11),  GPIO_INPUT, NULL},
	{"PD_TX_GND", GPIO_A, (1<<13),  GPIO_ODR_HIGH, NULL},
	{"LED_BLUE",    GPIO_B, (1<<6),  GPIO_OUT_LOW, NULL},
	{"LED_GREEN",   GPIO_B, (1<<7),  GPIO_OUT_LOW, NULL},

	{"TEST0", GPIO_C, (1<<0),  GPIO_OUT_LOW, NULL},
	{"TEST1", GPIO_C, (1<<1),  GPIO_OUT_LOW, NULL},
	{"TEST2", GPIO_C, (1<<2),  GPIO_OUT_LOW, NULL},

	/* Unimplemented signals which we need to emulate for now */
	GPIO_SIGNAL_NOT_IMPLEMENTED("CC_HOST"),
	GPIO_SIGNAL_NOT_IMPLEMENTED("ENTERING_RW"),
	GPIO_SIGNAL_NOT_IMPLEMENTED("WP_L"),
};
BUILD_ASSERT(ARRAY_SIZE(gpio_list) == GPIO_COUNT);

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* Pins with alternate functions */
const struct gpio_alt_func gpio_alt_funcs[] = {
	/* PD SPI: PA5:SCK PA11:MISO PA12:MOSI */
	{GPIO_A, 0x1820, GPIO_ALT_SPI, MODULE_USB_PD},
	/* USART1 on PA9/PA10 */
	{GPIO_A, 0x0600, GPIO_ALT_USART, MODULE_UART},
	/* PD Clock: PB12:TIM10_CH1 */
	{GPIO_B, 0x1000, GPIO_ALT_TIM9_11, MODULE_USB_PD},
};
const int gpio_alt_funcs_count = ARRAY_SIZE(gpio_alt_funcs);

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* USB PD RX. Converted to mV (3300mV/4096). */
	[ADC_CH_CC1_PD] = {"CC1_PD", 3300, 4096, 0, STM32_AIN(2)},
	[ADC_CH_CC2_PD] = {"CC2_PD", 3300, 4096, 0, STM32_AIN(2)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);
