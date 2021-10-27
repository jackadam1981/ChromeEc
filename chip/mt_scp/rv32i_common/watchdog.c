/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Watchdog driver */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "panic.h"
#include "registers.h"
#include "util.h"
#include "watchdog.h"
#include "scp_watchdog.h"

void watchdog_reload(void)
{
	SCP_CORE0_WDT_KICK = BIT(0);
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);

void disable_watchdog(void)
{
	/* disable watchdog */
	SCP_CORE0_WDT_CFG &= ~WDT_EN;
}

void enable_watchdog(void)
{
	const uint32_t timeout = WDT_PERIOD(CONFIG_WATCHDOG_PERIOD_MS);

	/* disable watchdog */
	SCP_CORE0_WDT_CFG &= ~WDT_EN;
	/* clear watchdog irq */
	SCP_CORE0_WDT_IRQ |= BIT(0);
	/* enable watchdog */
	SCP_CORE0_WDT_CFG = WDT_EN | timeout;
	/* reload watchdog */
	watchdog_reload();
}

int watchdog_init(void)
{
	enable_watchdog();

#ifdef CONFIG_PANIC_CONSOLE_OUTPUT
	{
		struct panic_data * panic = panic_get_data();

		if (panic == NULL && SCP_CORE0_MON_PC_LATCH == 0)
			return EC_SUCCESS;

		ccprintf("[Previous Panic]\n");
		if (panic) {
			panic_data_ccprint(panic);
		} else {
			ccprintf("No panic data\n");
		}
		ccprintf("Latch PC:%x LR:%x SP:%x\n",
			SCP_CORE0_MON_PC_LATCH,
			SCP_CORE0_MON_LR_LATCH,
			SCP_CORE0_MON_SP_LATCH);
	}
#endif

	return EC_SUCCESS;
}
