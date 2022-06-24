/* Copyright 2022 The ChromiumOS Authors.
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

static const struct button_config power_button = {
	.name = "power button",
	.gpio = GPIO_POWER_BUTTON_L,
	.debounce_us = BUTTON_DEBOUNCE_US,
	.flags = CONFIG_POWER_BUTTON_FLAGS,
};

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
	/* TODO
	 *	if (raw_power_button_pressed())
	 *	debounced_power_pressed = 1;
	 *
	 *	Enable interrupts, now that we've initialized
	 * gpio_enable_interrupt(power_button.gpio);
	 * /
}
