/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* IT8380 development board configuration */

#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "util.h"

/* Test GPIO interrupt function that toggles one LED. */
void test_interrupt(enum gpio_signal signal)
{
	static int led0_state = 0;

	/* toggle LED */
	led0_state = !led0_state;
	gpio_set_level(GPIO_L_LED0, led0_state);
}

/* GPIO signal list.  Must match order from enum gpio_signal. */
/*const*/ struct gpio_info gpio_list[] = {
	/* Unimplemented signals which we need to emulate for now */
	GPIO_SIGNAL_NOT_IMPLEMENTED("ENTERING_RW"),

	{"A0", GPIO_A, (1<<0), GPIO_OUTPUT | GPIO_LOW},
	{"A1", GPIO_A, (1<<1), GPIO_OUTPUT | GPIO_LOW},
	{"A2", GPIO_A, (1<<2), GPIO_OUTPUT | GPIO_LOW},
	{"A3", GPIO_A, (1<<3), GPIO_OUTPUT | GPIO_LOW},
	{"A4", GPIO_A, (1<<4), GPIO_OUTPUT | GPIO_LOW},
	{"A5", GPIO_A, (1<<5), GPIO_OUTPUT | GPIO_LOW},
	{"A6", GPIO_A, (1<<6), GPIO_OUTPUT | GPIO_LOW},
	{"A7", GPIO_A, (1<<7), GPIO_OUTPUT | GPIO_LOW},
	{"I0", GPIO_I, (1<<0), GPIO_OUTPUT | GPIO_LOW},
	{"I1", GPIO_I, (1<<1), GPIO_OUTPUT | GPIO_LOW},
	{"I2", GPIO_I, (1<<2), GPIO_OUTPUT | GPIO_LOW},
	{"I3", GPIO_I, (1<<3), GPIO_OUTPUT | GPIO_LOW},
	{"I4", GPIO_I, (1<<4), GPIO_OUTPUT | GPIO_LOW},
	{"I5", GPIO_I, (1<<5), GPIO_OUTPUT | GPIO_LOW},
	{"I6", GPIO_I, (1<<6), GPIO_OUTPUT | GPIO_LOW},
	{"I7", GPIO_I, (1<<7), GPIO_OUTPUT | GPIO_LOW},

	{"E0", GPIO_E, (1<<0), GPIO_INPUT | GPIO_INT_F_RISING, test_interrupt},
	{"E1", GPIO_E, (1<<1), GPIO_INPUT},
	{"E2", GPIO_E, (1<<2), GPIO_INPUT},
	{"E3", GPIO_E, (1<<3), GPIO_INPUT},
};
BUILD_ASSERT(ARRAY_SIZE(gpio_list) == GPIO_COUNT);

/* Pins with alternate functions */
/*const*/ struct gpio_alt_func gpio_alt_funcs[] = {
	{GPIO_B, 0x03, 1, MODULE_UART, GPIO_PULL_UP},	/* UART0 */
};
/*const*/ int gpio_alt_funcs_count = ARRAY_SIZE(gpio_alt_funcs);

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_SW_1);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

