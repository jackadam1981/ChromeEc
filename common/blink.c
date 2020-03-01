/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This is a sanity check program for boards that have LEDs and buttons. */

#include "common.h"
#include "gpio.h"
#include "hooks.h"

static int blink_enable = 1;
static const enum gpio_signal leds[] = { CONFIG_BLINK_LEDS };

BUILD_ASSERT(ARRAY_SIZE(leds) <= sizeof(int)*8, "Too many LEDs to drive.");

static void blink(void)
{
	static int led_values;

	if (blink_enable) {
		int i;
		for (i = 0; i < ARRAY_SIZE(leds); i++) {
			gpio_set_level(leds[i], BIT(i) & led_values);
		}
		led_values++;
	}
}
DECLARE_HOOK(HOOK_TICK, blink, HOOK_PRIO_DEFAULT);

#ifdef CONFIG_BLINK_BTN

static void btn_int_enable(void)
{
	gpio_enable_interrupt(CONFIG_BLINK_BTN);
}
DECLARE_HOOK(HOOK_INIT, btn_int_enable, HOOK_PRIO_DEFAULT);
DECLARE_DEFERRED(btn_int_enable);

__override
void CONFIG_BLINK_BTN_FN(enum gpio_signal signal)
{
	if (gpio_get_level(signal) == CONFIG_BLINK_BTN_ACT) {
		/* Debounce logic */
		gpio_disable_interrupt(CONFIG_BLINK_BTN);
		hook_call_deferred(&btn_int_enable_data,
				   CONFIG_BLINK_BTN_DEBOUNCE);

		blink_enable = !blink_enable;
	}
}

/* This will trigger in pre-processing, thus we can check at the bottom. */
#if !defined(CONFIG_BLINK_BTN_FN) || !defined(CONFIG_BLINK_BTN_ACT) || \
    !defined(CONFIG_BLINK_BTN_DEBOUNCE)
#	error There is missing configuration for CONFIG_BLINK_BTN.
#endif

#endif /* CONFIG_BLINK_BTN */

#ifndef CONFIG_BLINK_LEDS
#	error The macro CONFIG_BLINK_LEDS must be specified to use BLINK.
#endif
