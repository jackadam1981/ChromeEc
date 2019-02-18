/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Power managerment module for ISH */
#include "common.h"
#include "console.h"
#include "registers.h"
#include "interrupts.h"
#include "task.h"
#include "hooks.h"

#ifdef PM_DEBUG
#define CPUTS(outstr) cputs(CC_SYSTEM, outstr)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#else
#define CPUTS(outstr)
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

static void reset_prep_isr(void)
{
	/* Now in S0, reset MIA */
	REG32(IPC_ISH_RST_REG) = 0;
	REG32(IPC_ISH_RST_REG) = 1;
}
DECLARE_IRQ(ISH_RESET_PREP_IRQ, reset_prep_isr);

void pm_init(void)
{
	task_enable_irq(ISH_RESET_PREP_IRQ);

	REG32(IPC_ISH_RST_REG) = 0;
	/* clear reset history register in CCU */
	REG32(CCU_RST_HST) = REG32(CCU_RST_HST);
	REG32(PMU_RST_PREP) = 0;

	REG32(CCU_TCG_DISABLE) = 0;
	REG32(CCU_BCG_DISABLE) = 0;
}
DECLARE_HOOK(HOOK_INIT, pm_init, HOOK_PRIO_DEFAULT);
