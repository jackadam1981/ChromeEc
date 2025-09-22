/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "chipset.h"
#include "common.h"
#include "extpower.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "timer.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include <ap_power/ap_power.h>

#define INT_RECHECK_US 5000

static void board_backlight_handler(struct ap_power_ev_callback *cb,
				    struct ap_power_ev_data data)
{
	switch (data.event) {
	default:
		return;

	case AP_POWER_STARTUP:
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_en_ppvar_blpwr),
				1);
		break;

	case AP_POWER_HARD_OFF:
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_en_ppvar_blpwr),
				0);
		break;
	}
}

static void board_suspend_handler(struct ap_power_ev_callback *cb,
				  struct ap_power_ev_data data)
{
	const struct device *touchpad =
		DEVICE_DT_GET(DT_NODELABEL(hid_i2c_target));

	switch (data.event) {
	default:
		return;

	case AP_POWER_RESUME:
		i2c_target_driver_register(touchpad);
		break;

	case AP_POWER_SUSPEND:
		i2c_target_driver_unregister(touchpad);
		break;
	}
}

static int install_backlight_handler(void)
{
	static struct ap_power_ev_callback cb;
	static struct ap_power_ev_callback tp;
	/*
	 * Add a callback for start/hardoff to
	 * control the backlight load switch.
	 */
	ap_power_ev_init_callback(&cb, board_backlight_handler,
				  AP_POWER_STARTUP | AP_POWER_HARD_OFF);
	ap_power_ev_init_callback(&tp, board_suspend_handler,
				  AP_POWER_RESUME | AP_POWER_SUSPEND);
	ap_power_ev_add_callback(&cb);
	ap_power_ev_add_callback(&tp);

	return 0;
}

SYS_INIT(install_backlight_handler, APPLICATION, 1);
