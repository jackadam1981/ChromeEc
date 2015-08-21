/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Pure GPIO-based external power detection, buffered to PCH.
 * Drive high in S5-S0 when AC_PRESENT is high, otherwise drive low.
 */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "system.h"
#ifdef CONFIG_TEMP_SENSOR_TMP432
#include "tmp432.h"
#endif
#include "util.h"

int extpower_is_present(void)
{
	return gpio_get_level(GPIO_AC_PRESENT);
}

#ifdef CONFIG_TEMP_SENSOR_TMP432
static void tmp432_set_power_deferred(void)
{
	/* Shut tmp432 down if not in S0 && no external power */
	if (!extpower_is_present() && !chipset_in_state(CHIPSET_STATE_ON)) {
		if (EC_SUCCESS != tmp432_set_power(TMP432_POWER_OFF))
			ccprintf("ERROR: Can't shutdown TMP432.\n");
		return;
	}

	/*  else, turn it on. */
	if (EC_SUCCESS != tmp432_set_power(TMP432_POWER_ON))
		ccprintf("ERROR: Can't turn on TMP432.\n");
}
DECLARE_DEFERRED(tmp432_set_power_deferred);
#endif

static void extpower_buffer_to_soc(void)
{
	/* Drive high when AP is off */
	gpio_set_level(GPIO_LEVEL_SHIFT_EN_L,
		       chipset_in_state(CHIPSET_STATE_HARD_OFF) ? 1 : 0);
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, extpower_buffer_to_soc, HOOK_PRIO_DEFAULT);

static void extpower_shutdown(void)
{
	/* Disable level shift to SoC when shutting down */
	gpio_set_level(GPIO_LEVEL_SHIFT_EN_L, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, extpower_shutdown, HOOK_PRIO_DEFAULT);

void extpower_interrupt(enum gpio_signal signal)
{
	/* Trigger notification of external power change */
	extpower_buffer_to_soc();
#ifdef CONFIG_TEMP_SENSOR_TMP432
	hook_call_deferred(tmp432_set_power_deferred, 0);
#endif
}

static void extpower_init(void)
{
	extpower_buffer_to_soc();

	/* Enable interrupts, now that we've initialized */
	gpio_enable_interrupt(GPIO_AC_PRESENT);
}
DECLARE_HOOK(HOOK_INIT, extpower_init, HOOK_PRIO_DEFAULT);
