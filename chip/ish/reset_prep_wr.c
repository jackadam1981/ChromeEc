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

#define ISH_PMU_BASE      0x00800000
#define ISH_CCU_BASE      0x00900000

/* PMU Registers */
#define PMU_RST_PREP               (ISH_PMU_BASE + 0x5c)
#define PMU_RST_PREP_GET           (1 << 0)
#define PMU_RST_PREP_AVAIL         (1 << 1)
#define PMU_RST_PREP_INT_MASK      (1 << 31)
#define PMU_PMC_HOST_RST_CTL       (ISH_PMU_BASE + 0xfff20)
#define PMU_HOST_RST_B             (1 << 0)

/* CCU Registers */
#define CCU_TCG_EN                 (ISH_CCU_BASE + 0x0)
#define CCU_BCG_EN                 (ISH_CCU_BASE + 0x4)
#define CCU_RST_HST                (ISH_CCU_BASE + 0x34)
#define CCU_TCG_DISABLE            (ISH_CCU_BASE + 0x38)
#define CCU_BCG_DISABLE            (ISH_CCU_BASE + 0x3c)

static void reset_prep_wr_isr(void)
{
	/* Now in S0, reset MIA */
	REG32(WR_IPC_ISH_RST_REG) = 0;
	REG32(WR_IPC_ISH_RST_REG) = 1;
}
DECLARE_IRQ(WR_ISH_RESET_PREP_IRQ, reset_prep_wr_isr);

void reset_prep_init(void)
{
	task_enable_irq(WR_ISH_RESET_PREP_IRQ);

	REG32(WR_IPC_ISH_RST_REG) = 0;
	/* clear reset history register in CCU */
	REG32(CCU_RST_HST) = REG32(CCU_RST_HST);
	REG32(PMU_RST_PREP) = 0;

	REG32(CCU_TCG_DISABLE) = 0;
	REG32(CCU_BCG_DISABLE) = 0;
}
DECLARE_HOOK(HOOK_INIT, reset_prep_init, HOOK_PRIO_DEFAULT);
