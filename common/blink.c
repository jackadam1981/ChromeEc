/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This is a sanity check program for boards that have LEDs. */

#include "common.h"
#include "gpio.h"
#include "hooks.h"

static const enum gpio_signal leds[] = { CONFIG_BLINK_LEDS };

BUILD_ASSERT(ARRAY_SIZE(leds) <= sizeof(int)*8, "Too many LEDs to drive.");

static void blink(void)
{
	static int led_values;

	int i;
	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		gpio_set_level(leds[i], BIT(i) & led_values);
	}
	led_values++;
}
DECLARE_HOOK(HOOK_TICK, blink, HOOK_PRIO_DEFAULT);

#ifndef CONFIG_BLINK_LEDS
#	error The macro CONFIG_BLINK_LEDS must be specified to use BLINK.
#endif
