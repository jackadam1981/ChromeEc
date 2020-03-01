/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This is a sanity check program for boards that have LEDs and buttons. */

#include "common.h"
#include "gpio.h"
#include "hooks.h"

/* Initialize board. */
static void blink_init(void)
{
	gpio_enable_interrupt(GPIO_BTN1);
}
DECLARE_HOOK(HOOK_INIT, blink_init, HOOK_PRIO_DEFAULT);

static int blink_enable = 1;

static void blink(void)
{
	static int leds;

	if (blink_enable) {
		gpio_set_level(GPIO_LED1, BIT(0) & leds);
		gpio_set_level(GPIO_LED2, BIT(1) & leds);
		gpio_set_level(GPIO_LED3, BIT(2) & leds);
		leds++;
	}
}
DECLARE_HOOK(HOOK_TICK, blink, HOOK_PRIO_DEFAULT);

__override
void button_event(enum gpio_signal signal)
{
	if (gpio_get_level(GPIO_BTN1))
		blink_enable = !blink_enable;
}
