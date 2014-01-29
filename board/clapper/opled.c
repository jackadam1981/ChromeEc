/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery LED control for Rambi
 */

#include "charge_state.h"
#include "chipset.h"
#include "gpio.h"
#include "hooks.h"
#include "led_common.h"
#include "pwm.h"
#include "util.h"

#define PWRLEDON gpio_set_level(GPIO_POWER_LED_L, 0)
#define PWRLEDOFF gpio_set_level(GPIO_POWER_LED_L, 1)
#define PCHS3ACTIVE !gpio_get_level(GPIO_PCH_SLP_S3_L)
#define PCHS5ACTIVE !gpio_get_level(GPIO_PCH_SLP_S4_L)

/**
 * Called by hook task every 250 ms
 */
static void pled_tick(void)
{
	static unsigned ticks;
	if (ticks < 8) {	/* 2 SEC */
		ticks++;
	} else {
		ticks = 0;
	}
	if (PCHS3ACTIVE && !PCHS5ACTIVE) {	/* S3 */
		if (ticks < 4) {	/* turn on 1 SEC */
			PWRLEDON;
		} else {	/* turn off 1 SEC */
			PWRLEDOFF;
		}
	} else if (!PCHS3ACTIVE && !PCHS5ACTIVE) {	/* S0 */
		PWRLEDON;
	} else {	/* S5 */
		PWRLEDOFF;
	}
}
DECLARE_HOOK(HOOK_TICK, pled_tick, HOOK_PRIO_DEFAULT);
