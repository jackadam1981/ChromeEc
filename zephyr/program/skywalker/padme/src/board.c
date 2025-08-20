/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "chipset.h"
#include "common.h"
#include "driver/charger/rt9490.h"
#include "extpower.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "timer.h"

__override_proto void board_rt9490_adc_control(void);

#include <zephyr/drivers/gpio.h>

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

static int install_backlight_handler(void)
{
	static struct ap_power_ev_callback cb;
	/*
	 * Add a callback for start/hardoff to
	 * control the backlight load switch.
	 */
	ap_power_ev_init_callback(&cb, board_backlight_handler,
				  AP_POWER_STARTUP | AP_POWER_HARD_OFF);
	ap_power_ev_add_callback(&cb);

	return 0;
}

SYS_INIT(install_backlight_handler, APPLICATION, 1);

__overridable void board_rt9490_adc_control(void)
{
	rt9490_enable_adc(CHARGER_SOLO, extpower_is_present());
}

static void board_hook_ac_change(void)
{
	board_rt9490_adc_control();
}
DECLARE_HOOK(HOOK_AC_CHANGE, board_hook_ac_change, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_INIT, board_hook_ac_change, HOOK_PRIO_LAST);
