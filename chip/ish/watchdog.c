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
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "task.h"
#include "registers.h"
#include "system.h"
#include "watchdog.h"

/* Units are hundreds of milliseconds */
#define WDT_T1_PERIOD		(95) /* 9.5 seconds */
#define WDT_T2_PERIOD		(5)  /* 0.5 seconds */

int watchdog_init(void)
{
	/* Initialize WDT clock divider */
	CCU_WDT_CD = WDT_CLOCK_HZ * 100;

	/* Enable the watchdog timer and set initial T1/T2 values */
	WDT_CONTROL = WDT_CONTROL_ENABLE_BIT
		| (WDT_T2_PERIOD << 8)
		| WDT_T1_PERIOD;

	task_enable_irq(ISH_WDT_IRQ);
	return EC_SUCCESS;
}

void watchdog_warning_irq(void)
{
	ccprintf("WDT Warning!\n");

	/* Watchdog will bite in WDT_T2_PERIOD (500 ms) if not reloaded */
}
DECLARE_IRQ(ISH_WDT_IRQ, watchdog_warning_irq);

void watchdog_reload(void)
{
	/*
	 * ISH Supplemental Registers Info, 1.2.6.2:
	 * "When firmware writes a 1 to this bit, hardware reloads
	 * the values in WDT_T1 and WDT_T2..."
	 */
	WDT_RELOAD = 1;

	/* Hardware will then change WDT_RELOAD back to 0 */
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);

/* Console commmand for WDT testing and control */
static int command_wdt_control(int argc, char **argv)
{
	uint32_t t1, t2;

	if (argc < 2) {
		if (WDT_CONTROL & WDT_CONTROL_ENABLE_BIT)
			ccprintf("WDT Status: ENABLED (T1=%u, T2=%u)\n",
				 WDT_VALUES & 0xFF, (WDT_VALUES >> 8) & 0xFF);
		else
			ccprintf("WDT Status: DISABLED\n");
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "test")) {
		if (!(WDT_CONTROL & WDT_CONTROL_ENABLE_BIT)) {
			ccprintf("WDT is not enabled! 'wdt enable' first\n");
			return EC_ERROR_PARAM1;
		}

		while (1)
			if (WDT_VALUES & 0xFF)
				ccprintf("\rWait %u seconds... ",
					 (WDT_VALUES & 0xFF) / 10);

		/* Should never reach this */
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "disable")) {
		WDT_CONTROL &= ~WDT_CONTROL_ENABLE_BIT;
		task_disable_irq(ISH_WDT_IRQ);
		return command_wdt_control(0, NULL);
	}

	if (!strcasecmp(argv[1], "enable")) {
		t1 = (argc > 2) ? atoi(argv[2]) : WDT_T1_PERIOD;
		t2 = (argc > 3) ? atoi(argv[3]) : WDT_T2_PERIOD;
		watchdog_init();
		WDT_CONTROL = WDT_CONTROL_ENABLE_BIT | (t2 << 8) | t1;

		/* Wait for hardware to update WDT_VALUES */
		while ((WDT_VALUES & 0xFFFF) != (WDT_CONTROL & 0xFFFF))
			continue;

		return command_wdt_control(0, NULL);
	}

	return EC_ERROR_PARAM1;
}

DECLARE_CONSOLE_COMMAND(wdt, command_wdt_control,
			"[enable [T1 [T2]]|disable|test]",
			"View status, enable, disable, or test the watchdog");
