/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Watchdog Timer
 *
 * In ISH, there is a watchdog timer available from the hardware. It is
 * controlled by a few registers:
 *
 * - WDT_CONTROL (consists of enable bit, T1, and T2 values): When T1
 *   reaches 0, a warning is fired. After T2 then reaches 0, the system
 *   will reset.
 * - WDT_RELOAD: Pet the watchdog by setting to 1
 * - WDT_VALUES: Gives software access to T1 and T2 if needed
 *
 * For ISH implementation, we wish to reset only the ISH. Waiting until
 * T2 expires will kill the whole system. The functionality of T2 is
 * ignored, and we simply call system_reset when T1 expires. T2 will
 * only be used if the system cannot reset when T1 expires.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "task.h"
#include "registers.h"
#include "system.h"
#include "watchdog.h"

/* Units are hundreds of milliseconds */
#define WDT_T1_PERIOD		(100) /* 10 seconds */
#define WDT_T2_PERIOD		(10)  /* 1 second */

int watchdog_init(void)
{
	uint32_t reset_counter;

	/* Restore reset counter from always-on memory */
	reset_counter = watchdog_restore_reset_counter();

	/* If the last reset was caused by watchdog expiration... */
	if (system_get_reset_flags() & RESET_FLAG_WATCHDOG) {
		/* then increment the watchdog reset counter */
		reset_counter += 1;

		ccprints("Watchdog resets: %u", reset_counter);

		if (reset_counter >= CONFIG_WATCHDOG_MAX_RETRIES) {
			/* disable the watchdog if too many retries */
			ccprints("Maximum retries exceeded for watchdog reset."
				 " Watchdog disabled");
			return EC_SUCCESS;
		}
	} else {
		/* otherwise reset the counter to zero */
		reset_counter = 0;
	}
	watchdog_set_reset_counter(reset_counter);

	/* Initialize WDT clock divider */
	CCU_WDT_CD = WDT_CLOCK_HZ / 10; /* 10 Hz => 100 ms period */

	/* Enable the watchdog timer and set initial T1/T2 values */
	WDT_CONTROL = WDT_CONTROL_ENABLE_BIT
		| (WDT_T2_PERIOD << 8)
		| WDT_T1_PERIOD;

	task_enable_irq(ISH_WDT_IRQ);

	return EC_SUCCESS;
}

void watchdog_save_reset_counter(uint32_t value)
{
	ccprints("saving reset counter %u", value);
	ISH_WDT_RESET_COUNTER = value;
}

uint32_t watchdog_restore_reset_counter(void)
{
	ccprints("reset counter restored (%u)", ISH_WDT_RESET_COUNTER);
	return ISH_WDT_RESET_COUNTER;
}

void watchdog_reload(void)
{
	/*
	 * ISH Supplemental Registers Info, 1.2.6.2:
	 * "When firmware writes a 1 to this bit, hardware reloads
	 * the values in WDT_T1 and WDT_T2..."
	 */
	WDT_RELOAD = 1;
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);
