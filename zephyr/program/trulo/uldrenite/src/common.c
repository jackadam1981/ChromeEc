/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "system.h"
#include "timer.h"

/* Trigger shutdown by enabling the Z-sleep circuit */
__override void board_hibernate_late(void)
{
#ifndef CONFIG_PLATFORM_EC_HIBERNATE_PSL
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_slp_z), 1);
	/*
	 * The system should hibernate, but there may be
	 * a small delay, so return.
	 */
#endif
}

static void gpio_set_gpiod6_high_delay(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_gpiod6), 1);
}
DECLARE_DEFERRED(gpio_set_gpiod6_high_delay);

static void gpio_set_gpiod6_high(void)
{
	hook_call_deferred(&gpio_set_gpiod6_high_delay_data, 7 * USEC_PER_SEC);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, gpio_set_gpiod6_high, HOOK_PRIO_DEFAULT);

static void gpio_set_gpiod6_low(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_gpiod6), 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, gpio_set_gpiod6_low, HOOK_PRIO_DEFAULT);
