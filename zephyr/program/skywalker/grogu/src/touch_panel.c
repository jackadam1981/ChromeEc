/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "hooks.h"
#include "timer.h"

#include <zephyr/drivers/gpio.h>

static void en_tchscr_deferred(void)
{
	int level;

	level = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_tchscr_rst_3v3_odl));

	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_tchscr_en_3v3), level);
}
DECLARE_DEFERRED(en_tchscr_deferred);

void touch_screen_rst_interrupt(enum gpio_signal signal)
{
        hook_call_deferred(&en_tchscr_deferred_data, 120  * MSEC);
}
