/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "include/button.h"
#include "gpio.h"
#include "gpio/gpio.h"


static int debounced_power_pressed; /* Debounced power button state */
static int simulate_power_pressed;
static volatile int power_button_is_stable = 1;

int power_button_signal_asserted(void)
{
	/* TODO
	 * return !!(gpio_get_level(power_button.gpio)
	 *	== (power_button.flags & BUTTON_FLAG_ACTIVE_HIGH) ? 1 : 0);
	 */
	return 0;
}

/**
 * Handle power button initialization.
 */
static void power_button_init(void)
{
	const struct gpio_dt_spec *pwr_btn = GPIO_DT_FROM_NODE(
		DT_PHANDLE(DT_NODELABEL(btn_power_button_cfg), gpio));

	/* TODO */
	if (gpio_pin_get_dt(pwr_btn))
		debounced_power_pressed = 1;

	/* Enable interrupts, now that we've initialized
	 * gpio_enable_interrupt(power_button.gpio);
	 */
}

DECLARE_HOOK(HOOK_INIT, power_button_init, HOOK_PRIO_DEFAULT);
