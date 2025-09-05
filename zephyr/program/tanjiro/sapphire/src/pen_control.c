/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "gpio/gpio_int.h"
#include "gpio_signal.h"
#include "hooks.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>

#include <ap_power/ap_power.h>

static bool pen_pres_status = false;

static void set_wlc_power(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_pp5000_wlc_en),
			pen_pres_status);
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_pp1800_wlc_en),
			pen_pres_status);
}
DECLARE_DEFERRED(set_wlc_power);

void pen_pres_irq(enum gpio_signal signal)
{
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_pen_pres))) {
		pen_pres_status = false;
	} else {
		pen_pres_status = true;
	}

	if (!chipset_in_state(CHIPSET_STATE_ANY_OFF))
		hook_call_deferred(&set_wlc_power_data, 0);
}

static void pen_status_init(void)
{
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_pen_pres));
}
DECLARE_HOOK(HOOK_INIT, pen_status_init, HOOK_PRIO_DEFAULT);

static void board_backlight_handler(struct ap_power_ev_callback *cb,
				    struct ap_power_ev_data data)
{
	switch (data.event) {
	default:
		return;

	case AP_POWER_STARTUP:
		hook_call_deferred(&set_wlc_power_data, 0);
		break;
	case AP_POWER_SHUTDOWN_COMPLETE:
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_pp5000_wlc_en),
				false);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_pp1800_wlc_en),
				false);
		break;
	}
}

static int install_backlight_handler(void)
{
	static struct ap_power_ev_callback cb;

	/*
	 * Add a callback for suspend/resume to
	 * control the keyboard backlight.
	 */
	ap_power_ev_init_callback(&cb, board_backlight_handler,
				  AP_POWER_STARTUP |
					  AP_POWER_SHUTDOWN_COMPLETE);
	ap_power_ev_add_callback(&cb);
	return 0;
}

SYS_INIT(install_backlight_handler, APPLICATION, 1);
