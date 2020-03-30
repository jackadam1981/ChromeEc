/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks, PLL and power settings */

#include "clock.h"
#include "csr.h"
#include "registers.h"
#include "task.h"

static void ulposc_calibration(void)
{
}

void clock_init(void)
{
	/* reset hardware registers */
	SCP_CLK_SW_SEL = 0x0;
	SCP_CLK_DIV_SEL = 0x0;

	/* set Mux of VREQ to PMIC Wrap */
	SCP_CLK_CTRL_GENERAL_CTRL &= ~VREQ_PMIC_WRAP_SEL;
	/* set Mux of VREQ to PMIC */
	SCP_CPU_VREQ_CTRL =
		(SCP_CPU_VREQ_CTRL & ~VREQ_EXT_SEL) | VREQ_SEL;
	SCP_SEC_CTRL &= ~VREQ_SECURE_DIS;

	/* set Mux of VREQ to DVFSRC */
	SCP_CPU_VREQ_CTRL =
		(SCP_CPU_VREQ_CTRL & ~VREQ_DVFS_EXT_SEL) | VREQ_DVFS_SEL;

	SCP_SLEEP_CTRL =
		(SCP_SLEEP_CTRL & ~VREQ_COUNT_MASK) | VREQ_COUNT_VAL(1);

	/* set DDREN to auto mode */
	SCP_SYS_CTRL |= AUTO_DDREN;

	/* set settle time */
	SCP_CLK_SYS_VAL =
		(SCP_CLK_SYS_VAL & ~CLK_SYS_VAL_MASK) | CLK_SYS_VAL_VAL(1);
	SCP_CLK_HIGH_VAL =
		(SCP_CLK_HIGH_VAL & ~CLK_HIGH_VAL_MASK) | CLK_HIGH_VAL_VAL(1);

	ulposc_calibration();

	/* set sleep clk to DCM */
	SET_CSR(CSR_MCTREN, CSR_MCTREN_CG);

	/* set normal wake clock */
	SCP_WAKE_CKSW_SEL =
		(SCP_WAKE_CKSW_SEL & ~WAKE_CKSW_SEL_NORMAL_MASK) |
		WAKE_CKSW_SEL_NORMAL_VAL(0);

	/* enable fast wakeup support */
	SCP_CLK_CTRL_SLP_CTRL = 0x0;
	SCP_CLK_ON_CTRL =
		(SCP_CLK_ON_CTRL & ~HIGH_FINAL_VAL_MASK) |
		HIGH_FINAL_VAL_VAL(3);
	/* use fast wakeup latency 720us */
	SCP_FAST_WAKE_CNT_END =
		(SCP_FAST_WAKE_CNT_END & ~FAST_WAKE_CNT_END_MASK) |
		FAST_WAKE_CNT_END_VAL(0x18);
	/* wakeup clock select high div 26M */
	SCP_WAKE_CKSW_SEL =
		(SCP_WAKE_CKSW_SEL & ~WAKE_CKSW_SEL_SLOW_MASK) |
		WAKE_CKSW_SEL_SLOW_VAL(0x1);
	/* select ULPOSC1 as wakeup clock, mt8183 select ULPOSC2? */
	SCP_CLK_SEL_SLOW =
		(SCP_CLK_SEL_SLOW & ~CLK_SW_SEL_SLOW_MASK) |
		CLK_SW_SEL_SLOW_VAL(0x3);
	SCP_CLK_SEL_SLOW =
		(SCP_CLK_SEL_SLOW & ~CLK_DIVSW_SEL_SLOW_MASK) |
		CLK_DIVSW_SEL_SLOW_VAL(0);

	/* disable SPM controlled sleep mode. */
	SCP_SLEEP_CTRL &= ~SPM_SLP_MODE;

	/* Set legacy sleep control enable only once.
	 * Do not modify it during runtime.
	 */
	SCP_SLEEP_CTRL &= SLP_CTRL_EN;
	SCP_SLEEP_CTRL |= SLP_CTRL_EN;

#if 0
	task_enable_irq(SCP_IRQ_CLOCK);
	task_enable_irq(SCP_IRQ_CLOCK2);
#endif
}

#include "console.h"
#include "csr.h"
void __idle(void)
{
#if 1
	while (1) {
#if 0
		ccprints("%s: mie=%x", __func__, (unsigned int)READ_CSR_RAW(mie));
		ccprints("%s: mip=%x", __func__, (unsigned int)READ_CSR_RAW(mip));
		ccprints("%s: mstatus=%x", __func__, (unsigned int)READ_CSR_RAW(mstatus));
		ccprints("%s: mcause=%x", __func__, (unsigned int)READ_CSR_RAW(mcause));
		ccprints("%s: mctren=%x", __func__, (unsigned int)READ_CSR(CSR_MCTREN));
		ccprints("%s: mimask=%x", __func__, (unsigned int)READ_CSR(CSR_VIC_MIMASK_G0));
		ccprints("%s: milsel=%x", __func__, (unsigned int)READ_CSR(CSR_VIC_MILSEL_G0));
		ccprints("%s: miwakeup=%x", __func__, (unsigned int)READ_CSR(CSR_VIC_MIWAKEUP_G0));
		ccprints("%s: CSR_VIC_MICAUSE=%x", __func__, (unsigned int)READ_CSR(CSR_VIC_MICAUSE));
#endif

		cflush();
		asm("wfi");
	}
#endif
}

#if 0
void clock_control_irq(void)
{
	/* Read ack CLK_IRQ */
	(SCP_CLK_IRQ_ACK);
	task_clear_pending_irq(SCP_IRQ_CLK_CTRL);
}
DECLARE_IRQ(SCP_IRQ_CLK_CTRL, clock_control_irq, 3);

void clock_fast_wakeup_irq(void)
{
	/* Ack fast wakeup */
	//SCP_SLEEP_IRQ2 = 1;
	task_clear_pending_irq(SCP_IRQ_CLK_CTRL_2);
}
DECLARE_IRQ(SCP_IRQ_CLK_CTRL_2, clock_fast_wakeup_irq, 3);
#endif

void clock_refresh_console_in_use(void)
{
}
