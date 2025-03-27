/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "gpio/gpio_int.h"
#include "gpio_signal.h"
#include "hooks.h"

#include <zephyr/drivers/gpio.h>

void edp_bklt_interrupt(enum gpio_signal signal)
{
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_edp_bklt_en))) {
		gpio_set_level(GPIO_ENABLE_BACKLIGHT, 1);
	} else {
		gpio_set_level(GPIO_ENABLE_BACKLIGHT, 0);
	}
}

static void edp_bklt_init(void)
{
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_edp_bklt));
}
DECLARE_HOOK(HOOK_INIT, edp_bklt_init, HOOK_PRIO_DEFAULT);
