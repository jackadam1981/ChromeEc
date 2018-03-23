/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
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

#undef CONFIG_WATCHDOG_PERIOD_MS
#define CONFIG_WATCHDOG_PERIOD_MS 1000 /* T1 takes 1s to generate interrupt */
#define ISH_MAX_INTERRUPT 3 /* total 3 T1(3s) allowed, then reset */
static uint8_t wdt_counter;

/* WDT clock divider, make T1 interrupt happens in 3s */
#if defined(CHIP_FAMILY_ISH3) || defined(CHIP_FAMILY_ISH5) /* 120MHz/CD = 85Hz, 255 *(1/85) = 3s */
#define ISH_CCU_WDT_CD_VAL \
	(120000 * CONFIG_WATCHDOG_PERIOD_MS / ISH_WDT_CTL_T1)
#elif defined(CHIP_FAMILY_ISH4)
#define ISH_CCU_WDT_CD_VAL \
	(100000 * CONFIG_WATCHDOG_PERIOD_MS / ISH_WDT_CTL_T1)
#endif

static void wdt_reload(void) {
	ISH_WDT_REG(ISH_WDT_RL) |= ISH_WDT_RL_VAL;
	while (ISH_WDT_REG(ISH_WDT_RL) & ISH_WDT_RL_VAL);
}

void watchdog_reload(void)
{
	/* Disable watchdog interrupt */
	task_disable_irq(ISH_WDT_IRQ);

	/* Reload and reset counter*/
	wdt_reload();
	wdt_counter = 0;
	/* Enable watchdog interrupt */
	task_enable_irq(ISH_WDT_IRQ);
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);

static void wdt_interrupt(void)
{
	if (wdt_counter < ISH_MAX_INTERRUPT) {
		wdt_reload(); /*reload but NOT reset counter*/
		wdt_counter++;
		ccprintf("Watchdog warning(%d)!!!\n", wdt_counter);
	}
	/* Continuous 3s hook tick doesn't reload WDT, let ISH die */
}

DECLARE_IRQ(ISH_WDT_IRQ, wdt_interrupt);

int watchdog_init(void)
{
	wdt_counter = 0;

	task_enable_irq(ISH_WDT_IRQ);
	ISH_CCU_REG(ISH_CCU_WDT_CD) = ISH_CCU_WDT_CD_VAL;
	/* Enable watchdog timer, set T1&T2 counter */
	ISH_WDT_REG(ISH_WDT_CTL) |=
		(ISH_WDT_CTL_EN | ISH_WDT_CTL_T2 | ISH_WDT_CTL_T1);

	return EC_SUCCESS;
}
