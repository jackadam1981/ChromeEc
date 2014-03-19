/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Fruitpie board configuration */

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
	{"AIN1", GPIO_A, (1<<1),  GPIO_ANALOG, NULL},
	{"AIN2", GPIO_A, (1<<2),  GPIO_ANALOG, NULL},
	{"AIN3", GPIO_A, (1<<3),  GPIO_ANALOG, NULL},
	{"AIN4", GPIO_A, (1<<4),  GPIO_ANALOG, NULL},
	{"AIN5", GPIO_A, (1<<5),  GPIO_ANALOG, NULL},
	{"AIN6", GPIO_A, (1<<6),  GPIO_ANALOG, NULL},
	{"AIN7", GPIO_A, (1<<7),  GPIO_ANALOG, NULL},
	{"AIN8", GPIO_B, (1<<0),  GPIO_ANALOG, NULL},
	{"AIN9", GPIO_B, (1<<1),  GPIO_ANALOG, NULL},
	/* Outputs */
	{"LED_BLUE",    GPIO_B, (1<<6),  GPIO_OUT_LOW, NULL},
	{"LED_GREEN",   GPIO_B, (1<<7),  GPIO_OUT_LOW, NULL},

	/* Unimplemented signals which we need to emulate for now */
	GPIO_SIGNAL_NOT_IMPLEMENTED("ENTERING_RW"),
	GPIO_SIGNAL_NOT_IMPLEMENTED("WP_L"),
};
BUILD_ASSERT(ARRAY_SIZE(gpio_list) == GPIO_COUNT);

/* ADC channels */
const struct adc_t adc_channels[] = {
	[ADC_CH_AIN0] = {"PA0", 3300, 4096, 0, STM32_AIN(0)},
	[ADC_CH_AIN1] = {"PA1", 3300, 4096, 0, STM32_AIN(1)},
	[ADC_CH_AIN2] = {"PA2", 3300, 4096, 0, STM32_AIN(2)},
	[ADC_CH_AIN3] = {"PA3", 3300, 4096, 0, STM32_AIN(3)},
	[ADC_CH_AIN4] = {"PA4", 3300, 4096, 0, STM32_AIN(4)},
	[ADC_CH_AIN5] = {"PA5", 3300, 4096, 0, STM32_AIN(5)},
	[ADC_CH_AIN6] = {"PA6", 3300, 4096, 0, STM32_AIN(6)},
	[ADC_CH_AIN7] = {"PA7", 3300, 4096, 0, STM32_AIN(7)},
	[ADC_CH_AIN8] = {"PB0", 3300, 4096, 0, STM32_AIN(8)},
	[ADC_CH_AIN9] = {"PB1", 3300, 4096, 0, STM32_AIN(9)},
};

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* Pins with alternate functions */
const struct gpio_alt_func gpio_alt_funcs[] = {
	{GPIO_A, 0xC000, 1, MODULE_UART}, /* USART2: PA14/PA15 */
};
const int gpio_alt_funcs_count = ARRAY_SIZE(gpio_alt_funcs);
