/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "gpio/gpio_int.h"
#include "gpio_signal.h"
#include "hooks.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>

static bool value_en;

static void set_pp3300_tp_en(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_pp3300_tp), value_en);
}
DECLARE_DEFERRED(set_pp3300_tp_en);

void soc_bl_interrupt(enum gpio_signal signal)
{
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_soc_bl_en))) {
		value_en = true;
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_bl_en_od), 1);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_tchscr_en), 1);
		hook_call_deferred(&set_pp3300_tp_en_data, 11 * USEC_PER_MSEC);
	} else {
		value_en = false;
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_bl_en_od), 0);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_tchscr_en), 0);
		hook_call_deferred(&set_pp3300_tp_en_data, 0);
	}
}

static void ap_bl_en_init(void)
{
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_soc_bl_en));
}
DECLARE_HOOK(HOOK_INIT, ap_bl_en_init, HOOK_PRIO_DEFAULT);
