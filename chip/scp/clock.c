/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks, PLL and power settings */

#include "clock.h"
#include "common.h"
#include "registers.h"
#include "task.h"
#include "util.h"

void clock_init(void)
{
	/* Set VREQ to HW mode */
	SCP_CPU_VREQ = 0x10001;
	SCP_SECURE_CTRL &= ~ENABLE_SPM_MASK_VREQ;

	/* Set DDREN auto mode */
	SCP_SYS_CTRL |= AUTO_DDREN;

	/* Set settle time (set all clock counter to 1) */
	SCP_CLK_SYS_VAL &= ~CLK_COUNTER_MASK;
	SCP_CLK_SYS_VAL |= 1;
	SCP_CLK_HIGH_VAL &= ~CLK_COUNTER_MASK;
	SCP_CLK_HIGH_VAL |= 1;
	SCP_CLK_SLEEP_CTRL &= ~VREQ_COUNTER_MASK;
	SCP_CLK_SLEEP_CTRL |= VREQ_COUNTER_VAL(1);

	/* Set normal wake clock */
	SCP_WAKE_CKSW &= ~WAKE_CKSW_SEL_NORMAL_MASK;

	/* Enable fast wakeup support */
	SCP_CLK_SLEEP = 0;
	SCP_CLK_ON_CTRL &= ~HIGH_FINAL_VAL_MASK;
	SCP_CLK_ON_CTRL |= 0x300;
	SCP_FAST_WAKE_CNT_END &= 0xfffff000;
	SCP_FAST_WAKE_CNT_END |= 0x18;

	/* Set slow wake clock */
	SCP_WAKE_CKSW &= ~WAKE_CKSW_SEL_SLOW_MASK;
	SCP_WAKE_CKSW |= 0x10;

	/* Select CLK_HIGH as wakeup clock */
	SCP_CLK_SLOW_SEL &= ~CKSW_SEL_SLOW_MASK;
	SCP_CLK_SLOW_SEL |= 3;
	SCP_CLK_SLOW_SEL &= ~CKSW_SEL_SLOW_DIV_MASK;

	/* Set legacy wakeup
	 *   - disable SPM sleep control
	 *   - disable SCP sleep mode
	 */
	SCP_CLK_SLEEP_CTRL &= ~(EN_SLEEP_CTRL | SPM_SLEEP_MODE);

	task_enable_irq(SCP_IRQ_CLOCK);
	task_enable_irq(SCP_IRQ_CLOCK2);
}

void clock_control_irq(void)
{
	/* Read ack CLK_IRQ */
	(SCP_CLK_IRQ_ACK);
}
DECLARE_IRQ(SCP_IRQ_CLOCK, clock_control_irq, 3);

void clock_fast_wakeup_irq(void)
{
	/* Ack fast wakeup */
	SCP_SLEEP_IRQ2 = 1;
}
DECLARE_IRQ(SCP_IRQ_CLOCK2, clock_fast_wakeup_irq, 3);

