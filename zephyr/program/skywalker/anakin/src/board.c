/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio/gpio_int.h"
#include "hooks.h"
#include "power.h"
#include "timer.h"

#include <zephyr/drivers/gpio.h>

#include <ap_power/ap_power.h>

#define INT_RECHECK_US 5000

static void bl_pg_handle(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_en_ppvar_blpwr), 1);
	k_msleep(50);
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_bl_pwr_pgd))) {
		gpio_enable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_bl_pwr_pgd));
	}
}
DECLARE_DEFERRED(bl_pg_handle);

void bl_pg_interrupt(enum gpio_signal s)
{
	if (!gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_bl_pwr_pgd))) {
		gpio_disable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_bl_pwr_pgd));
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_en_ppvar_blpwr),
				0);
		hook_call_deferred(&bl_pg_handle_data, 10 * USEC_PER_MSEC);
	}
}

static void board_backlight_handler(struct ap_power_ev_callback *cb,
				    struct ap_power_ev_data data)
{
	switch (data.event) {
	default:
		return;

	case AP_POWER_STARTUP:
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_en_ppvar_blpwr),
				1);
		gpio_enable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_bl_pwr_pgd));
		break;

	case AP_POWER_HARD_OFF:
		gpio_disable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_bl_pwr_pgd));
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
	 * control the backlight load swith.
	 */
	ap_power_ev_init_callback(&cb, board_backlight_handler,
				  AP_POWER_STARTUP | AP_POWER_HARD_OFF);
	ap_power_ev_add_callback(&cb);

	return 0;
}

SYS_INIT(install_backlight_handler, APPLICATION, 1);

static void check_audio_jack(void)
{
	enum power_state powerstate;

	powerstate = power_get_state();

	if (powerstate == POWER_S0 || powerstate == POWER_S3S0) {
		if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_jd1)))
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(gpio_5p0va_pwr_mode), 0);
		else
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(gpio_5p0va_pwr_mode), 1);
	} else {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_5p0va_pwr_mode), 0);
	}
}
DECLARE_DEFERRED(check_audio_jack);

DECLARE_HOOK(HOOK_INIT, check_audio_jack, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_RESUME, check_audio_jack, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, check_audio_jack, HOOK_PRIO_DEFAULT);

void audio_jack_interrupt(enum gpio_signal s)
{
	hook_call_deferred(&check_audio_jack_data, INT_RECHECK_US);
}

static void board_setup_init()
{
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_jd1));
}
DECLARE_HOOK(HOOK_INIT, board_setup_init, HOOK_PRIO_PRE_DEFAULT);
