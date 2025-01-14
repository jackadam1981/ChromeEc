/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "console.h"
#include "extpower.h"
#include "gpio/gpio_int.h"
#include "hooks.h"

void ac_feedback_lcd(void)
{
	if (extpower_is_present()) {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ac_lcd), 1);
	} else {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ac_lcd), 0);
	}
}
DECLARE_HOOK(HOOK_AC_CHANGE, ac_feedback_lcd, HOOK_PRIO_DEFAULT);

static bool value_en;

static void set_bl_en_pin(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_h1), value_en);
}
DECLARE_DEFERRED(set_bl_en_pin);

void ap_bl_en_interrupt(enum gpio_signal signal)
{
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_lcd_rst))) {
		value_en = true;
		hook_call_deferred(&set_bl_en_pin_data, 0);
	} else {
		value_en = false;
		hook_call_deferred(&set_bl_en_pin_data, 50 * MSEC);
	}
}

static void ap_bl_en_init(void)
{
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_gpio_lcd_rst));
}
DECLARE_HOOK(HOOK_INIT, ap_bl_en_init, HOOK_PRIO_DEFAULT);
