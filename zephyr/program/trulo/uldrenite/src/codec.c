/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "timer.h"

static void gpio_set_ecmute_high_delay(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_mute), 1);
}
DECLARE_DEFERRED(gpio_set_ecmute_high_delay);

static void gpio_set_ecmute_high(void)
{
	hook_call_deferred(&gpio_set_ecmute_high_delay_data, 7 * USEC_PER_SEC);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, gpio_set_ecmute_high, HOOK_PRIO_DEFAULT);

static void gpio_set_ecmute_low(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_mute), 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, gpio_set_ecmute_low, HOOK_PRIO_DEFAULT);
