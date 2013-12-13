/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Norrin EC-less board configuration */

#include "gpio.h"
#include "i2c.h"
#include "registers.h"
#include "util.h"

/* GPIO signal list.  Must match order from enum gpio_signal. */
const struct gpio_info gpio_list[] = {
	/* Unimplemented signals which we need to emulate for now */
	GPIO_SIGNAL_NOT_IMPLEMENTED("RECOVERYn"),
	GPIO_SIGNAL_NOT_IMPLEMENTED("WP"),
	GPIO_SIGNAL_NOT_IMPLEMENTED("ENTERING_RW"),
};
BUILD_ASSERT(ARRAY_SIZE(gpio_list) == GPIO_COUNT);

/* Pins with alternate functions */
const struct gpio_alt_func gpio_alt_funcs[] = {
/*	{GPIO_PORT(0), 0x24,     1, MODULE_UART}, *//* UART0 */
};
const int gpio_alt_funcs_count = ARRAY_SIZE(gpio_alt_funcs);
