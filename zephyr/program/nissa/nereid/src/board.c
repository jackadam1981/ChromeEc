/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"

#include <zephyr/devicetree.h>

#include <ap_power/ap_power.h>

static void board_power_change(struct ap_power_ev_callback *cb,
			       struct ap_power_ev_data data)
{
	/*
	 * Enable power to pen garage when system is active (safe even if no
	 * pen is present).
	 */
	const struct gpio_dt_spec *const pen_power_gpio =
		GPIO_DT_FROM_NODELABEL(gpio_en_pp5000_pen_x);

	switch (data.event) {
	case AP_POWER_STARTUP:
		gpio_pin_set_dt(pen_power_gpio, 1);
		break;
	case AP_POWER_SHUTDOWN:
		gpio_pin_set_dt(pen_power_gpio, 0);
		break;
	default:
		break;
	}
}

static void board_setup_init(void)
{
	static struct ap_power_ev_callback cb;

	ap_power_ev_init_callback(&cb, board_power_change,
				  AP_POWER_STARTUP | AP_POWER_SHUTDOWN);
	ap_power_ev_add_callback(&cb);
}
DECLARE_HOOK(HOOK_INIT, board_setup_init, HOOK_PRIO_INIT_I2C);
