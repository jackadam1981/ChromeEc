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

/* Test interrupt function. */
void c1_interrupt(enum gpio_signal signal)
{
	panic_puts("|");
}

/* GPIO signal list.  Must match order from enum gpio_signal. */
/*const*/ struct gpio_info gpio_list[] = {
	/* Unimplemented signals which we need to emulate for now */
	GPIO_SIGNAL_NOT_IMPLEMENTED("ENTERING_RW"),
	{"C2", GPIO_C, (1<<2), GPIO_OUTPUT | GPIO_LOW},
	{"C1", GPIO_C, (1<<1), GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_F_RISING, c1_interrupt},
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
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

