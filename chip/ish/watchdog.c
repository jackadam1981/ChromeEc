/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Watchdog driver */

#include "common.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "watchdog.h"

 /* T1 takes 9,5s (95 * 100MSEC) to generate interrupt */
#undef CONFIG_WATCHDOG_PERIOD_MS
#define CONFIG_WATCHDOG_PERIOD_MS (10 * 1000)

/* WDT clock divider */
#if defined(CHIP_FAMILY_ISH3) || defined(CHIP_FAMILY_ISH5)
#define ISH_CCU_WDT_CD_VAL \
	(120000 * CONFIG_WATCHDOG_PERIOD_MS / (ISH_WDT_CTL_T1 + ISH_WDT_CTL_T2))
#elif defined(CHIP_FAMILY_ISH4)
#define ISH_CCU_WDT_CD_VAL \
	(100000 * CONFIG_WATCHDOG_PERIOD_MS / (ISH_WDT_CTL_T1 + ISH_WDT_CTL_T2))
#endif

static void wdt_reload(void)
{
	ISH_WDT_REG(ISH_WDT_RL) |= ISH_WDT_RL_VAL;

	/*
	 * Waiting for hardware confirmation that the watch dog timer actually
	 * reloaded
	 */
	while (ISH_WDT_REG(ISH_WDT_RL) & ISH_WDT_RL_VAL)
		;
}

void watchdog_reload(void)
{
	/* Disable watchdog interrupt */
	task_disable_irq(ISH_WDT_IRQ);

	/* Reload and reset counter*/
	wdt_reload();

	/* Enable watchdog interrupt */
	task_enable_irq(ISH_WDT_IRQ);
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);

static void wdt_interrupt(void)
{
	/* Give warning and a chance to dump any data here*/
	ccprintf("Watchdog warning!\n");

	/* Reserved to dump any data here before watchdog bites in 500ms */
}

DECLARE_IRQ(ISH_WDT_IRQ, wdt_interrupt);

int watchdog_init(void)
{
	task_enable_irq(ISH_WDT_IRQ);
	ISH_CCU_REG(ISH_CCU_WDT_CD) = ISH_CCU_WDT_CD_VAL;
	/*
	 * Enable watchdog timer, set T1&T2 counter
	 * T2 doesn't start until T1 expires
	 * T1: 9.5s (bark)
	 * T2: 0.5s (bite)
	 */
	ISH_WDT_REG(ISH_WDT_CTL) |=
		(ISH_WDT_CTL_EN | (ISH_WDT_CTL_T2 << 8) | ISH_WDT_CTL_T1);

	return EC_SUCCESS;
}
