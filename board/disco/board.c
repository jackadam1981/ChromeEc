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
	static int x = 0;
	x ^= 1;
	gpio_set_level(GPIO_GREEN_LED, x);
	gpio_set_level(GPIO_BLUE_LED, !x);
}

/* GPIO signal list.  Must match order from enum gpio_signal. */
const struct gpio_info gpio_list[] = {
	/* Inputs with interrupt handlers are first for efficiency */
	{"USER_BUTTON", GPIO_A, (1<<0),  GPIO_INT_BOTH, button_event},
	/* Output */
	{"GREEN_LED",   GPIO_B, (1<<7),  GPIO_OUT_LOW, NULL},
	{"BLUE_LED",    GPIO_B, (1<<6),  GPIO_OUT_HIGH, NULL},

	/* Unimplemented signals which we need to emulate for now */
	GPIO_SIGNAL_NOT_IMPLEMENTED("ENTERING_RW"),
	GPIO_SIGNAL_NOT_IMPLEMENTED("WP_L"),
};
BUILD_ASSERT(ARRAY_SIZE(gpio_list) == GPIO_COUNT);

/* ADC channels */
const struct adc_t adc_channels[] = {
};

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* Pins with alternate functions */
const struct gpio_alt_func gpio_alt_funcs[] = {
	{GPIO_A, 0x0600, 4, MODULE_UART}, /* USART1: PA9/PA10 */
};
const int gpio_alt_funcs_count = ARRAY_SIZE(gpio_alt_funcs);
