/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "common.h"
#include "console.h"
#include "link_defs.h"
#include "panic.h"
#include "power_mgt.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "task_defs.h"
#include "interrupts.h"
#include "ipc.h"
#include "hpet.h"


#ifdef CONFIG_LOW_POWER_IDLE

int pm_d0i1(void)
{
/*
* pm_d0i1:
* This function performs the necessary preparations to enter d0i1.
* This involves:
* 1. Checking which drivers have active transactions.
* 2. Configuring CCU for trunk clock gating if possible.
*/
	REG32(CCU_TRUNK_CG) = CCU_TRUNK_CG_EN;  /* for KBL */

        /* TODO: wdt_disable(); */

        CPU_ENTER_IDLE();
        /* back here after processing the ISR */

        return 0;
}

int pm_d0i2(void)
{
        /* find way to calculate SRAM banks to be into retention mode */
	return 0;
}


int pm_d0i3(void)
{
        return 0;
}


void pm_init(void)
{
	/* Clear reset history register in CCU */
        REG32(IPC_ISH_RST_REG) = 0;
        REG32(CCU_RST_HST) = REG32(CCU_RST_HST); // Write Clear, will volotile help ?


        /* ISH_CONFIG_SUPPORT_SIDEBAND for both ISH3 and ISH4 */
        /* Turn on dynamic clock gating for SB */
        REG32(SBEP_REG_CLK_GATE_ENABLE) =
                  (SB_CLK_GATE_EN_LOCAL_CLK_GATE | SB_CLK_GATE_EN_TRUNK_CLK_GATE);

        /* keep upper 16bit to avoid ish function clock CG abort issue */
        REG32(PMU_ISH_FABRIC_CNT) = (REG32(PMU_ISH_FABRIC_CNT) & 0xffff0000) | 50;
        REG32(CCU_TCG_DISABLE) = 0x0;  /* enable trunk CG */
        REG32(CCU_BCG_DISABLE) = 0x0;  /* enable block CG */
        REG32(PMU_RF_ROM_PWR_CTRL) = PMU_RF_ROM_PG_DIS;

        /* TODO: AON task init. Need for D0i3 implementation. */
        /* TODO: init_aon_task(); */
}

/* param: length_of_idle :  counts, not in uSec */
void pm_execute_idle_flow(int32_t length_of_idle)
{
	/*
	 * TODO: select proper D0ix(D0i1, D0i2, D0i3..) from 'length_of_idle'
	 * eventually
	 */

        pm_d0i1();

}

void pm_return_from_idle(void)
{
        REG32(CCU_TRUNK_CG) = 0;

        /*TODO: wdt_enable();
                wdt_reload(); */
}

#endif /* !CONFIG_LOW_POWER_IDLE */
