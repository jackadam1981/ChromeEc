/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks, PLL and power settings */

#include "clock_chip.h"
#include "clock.h"
#include "csr.h"
#include "registers.h"

static void clock_configure_ulposc(int osc, uint32_t osc_div, uint32_t osc_cali)
{

}

static void clock_high_enable(int osc)
{

}

/**
 * Calibrate ULPOSC to target frequency.
 *
 * @param osc           0:ULPOSC1, 1:ULPOSC2
 * @param target_mhz    Target frequency to set
 * @return              Frequency counter output
 *
 */
static int clock_calibrate_ulposc(int osc, int target_mhz)
{
	return 0;
}

void clock_select_clock(enum scp_clock_source src)
{
	/*
	 * DIV2 divider takes precedence over clock selection to prevent
	 * over-clocking.
	 */
	if (src == SCP_CLK_ULPOSC1)
		SCP_CLK_DIV_SEL = CLK_DIV_SEL2;

	SCP_CLK_SW_SEL = src;

	if (src != SCP_CLK_ULPOSC1)
		SCP_CLK_DIV_SEL = CLK_DIV_SEL1;
}

void clock_init(void)
{
	/* select default 26M system clock */
	clock_select_clock(SCP_CLK_26M);

	/* set VREQ to HW mode */
	SCP_CPU_VREQ_CTRL = VREQ_SEL | VREQ_DVFS_SEL;
	//SCP_SECURE_CTRL &= ~ENABLE_SPM_MASK_VREQ;

	/* set DDREN to auto mode */
	SCP_SYS_CTRL |= AUTO_DDREN;

	/* set settle time */
	SCP_CLK_SYS_VAL =
		(SCP_CLK_SYS_VAL & ~CLK_SYS_VAL_MASK) | CLK_SYS_VAL_VAL(1);
	SCP_CLK_HIGH_VAL =
		(SCP_CLK_HIGH_VAL & ~CLK_HIGH_VAL_MASK) | CLK_HIGH_VAL_VAL(1);
	SCP_SLEEP_CTRL =
		(SCP_SLEEP_CTRL & ~VREQ_COUNT_MASK) | VREQ_COUNT_VAL(1);

#if 0
	/* Disable slow wake */
	SCP_CLK_SLEEP = SLOW_WAKE_DISABLE;
	/* Disable SPM sleep control, disable sleep mode */
	SCP_CLK_SLEEP_CTRL &= ~(SPM_SLEEP_MODE | EN_SLEEP_CTRL);
#endif

	/* Turn off ULPOSC2 */
	SCP_CLK_ON_CTRL |= HIGH_CORE_DIS_SUB;
	clock_configure_ulposc(0, 12, 32);
	clock_high_enable(0); /* Turn on ULPOSC1 */
	clock_configure_ulposc(1, 16, 32);
	clock_high_enable(1); /* Turn on ULPOSC2 */

	/* Calibrate ULPOSC */
	clock_calibrate_ulposc(0, ULPOSC1_CLOCK_MHZ);
	clock_calibrate_ulposc(1, ULPOSC2_CLOCK_MHZ);

	/* Enable default clock gate */
	SCP_SET_CLK_CG |= CG_DMA_CH3 | CG_DMA_CH2 | CG_DMA_CH1 | CG_DMA_CH0 |
		CG_I2C_MCLK | CG_MAD_MCLK | CG_AP2P_MCLK;

#if 0
	/* Select pwrap_ulposc */
	AP_CLK_CFG_5 = (AP_CLK_CFG_5 & ~PWRAP_ULPOSC_MASK) | OSC_D16;

	/* Enable pwrap_ulposc clock gate */
	AP_CLK_CFG_5_CLR = PWRAP_ULPOSC_CG;

	/* Set normal wake clock */
	SCP_WAKE_CKSW &= ~WAKE_CKSW_SEL_NORMAL_MASK;
#endif

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
	/* set normal wake clock */
	/* select ULPOSC1 as wakeup clock, mt8183 select ULPOSC2? */
	SCP_CLK_SEL_SLOW =
		(SCP_CLK_SEL_SLOW & ~CLK_SW_SEL_SLOW_MASK) |
		CLK_SW_SEL_SLOW_VAL(0x3);
	SCP_CLK_SEL_SLOW =
		(SCP_CLK_SEL_SLOW & ~CLK_DIVSW_SEL_SLOW_MASK) |
		CLK_DIVSW_SEL_SLOW_VAL(0);

	/*
	 * Set legacy wakeup
	 *   - disable SPM sleep control
	 *   - disable SCP sleep mode
	 *
	 * Set legacy sleep control enable only once.
	 * Do not modify it during runtime.
	 */
	SCP_SLEEP_CTRL &= ~(SLP_CTRL_EN | SPM_SLP_MODE);

#if 0
	task_enable_irq(SCP_IRQ_CLOCK);
	task_enable_irq(SCP_IRQ_CLOCK2);
#endif
}
