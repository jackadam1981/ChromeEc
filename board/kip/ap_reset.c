/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "timer.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_HOOK, outstr)
#define CPRINTF(format, args...) cprintf(CC_HOOK, format, ## args)

static void modem_power_on(void)
{
	CPRINTF("[%T GPIO_PP3300_LTE_EN %d->1]\n",
		gpio_get_level(GPIO_PP3300_LTE_EN));
	gpio_set_level(GPIO_PP3300_LTE_EN, 1);
}
DECLARE_DEFERRED(modem_power_on);

static void modem_power_cycle(void)
{
	CPRINTF("[%T GPIO_PP3300_LTE_EN %d->0]\n",
		gpio_get_level(GPIO_PP3300_LTE_EN));
	gpio_set_level(GPIO_PP3300_LTE_EN, 0);
	hook_call_deferred(modem_power_on, 30 * MSEC);
}
DECLARE_HOOK(HOOK_CHIPSET_WARM_RESET, modem_power_cycle, HOOK_PRIO_DEFAULT);
