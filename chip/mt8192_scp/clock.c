/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks, PLL and power settings */

#include <assert.h>
#include <string.h>

#include "clock_chip.h"
#include "clock.h"
#include "console.h"
#include "csr.h"
#include "registers.h"
#include "timer.h"

#define CPRINTF(format, args...) cprintf(CC_CLOCK, format, ## args)

static struct opp_ulposc_cfg opp[] = {
	{ .osc = 1, .target_mhz = 196, .div = 20, .fband = 10, .mod = 3, .cali = 64, },
	{ .osc = 0, .target_mhz = 260, .div = 14, .fband = 2, .mod = 0, .cali = 64, },
	{ .osc = 1, .target_mhz = 280, .div = 20, .fband = 2, .mod = 0, .cali = 64, },
	{ .osc = 1, .target_mhz = 360, .div = 20, .fband = 10, .mod = 0, .cali = 64, },
};

static inline void busy_udelay(int usec)
{
	/*
	 * Delaying by busy-looping, for place that can't use udelay because of
	 * the clock not configured yet. The value 28 is chosen approximately
	 * from experiment.
	 */
	volatile int i = usec * 28;

	while (i--)
		;
}

static void config_ulposc_default(struct opp_ulposc_cfg *opp)
{
	unsigned int val;

	/* Clear all bits */
	val = 0;
	/* cp_en = 0 */
	val &= ~OSC_CP_EN;
	/* Set div */
	val |= opp->div << OSC_DIV_SHIFT;
	/* Set F-band; I-band = 82 */
	val |= (opp->fband << OSC_FBAND_SHIFT) | (82 << OSC_IBAND_SHIFT);
	/* Set calibration */
	val |= opp->cali;
	/* Set control register 0 */
	AP_ULPOSC_CON0(opp->osc) = val;

	/* Clear all bits */
	val = 0;
	/* div2_en = 0 */
	val &= ~OSC_DIV2_EN;
	/* Set mod */
	val |= opp->mod << OSC_MOD_SHIFT;
	/* rsv2 = 0, rsv1 = 41, cali_32k = 0 */
	val |= 41 << OSC_RSV1_SHIFT;
	/* Set control register 1 */
	AP_ULPOSC_CON1(opp->osc) = val;

	/* Clear all bits */
	val = 0;
	/* bias = 64 */
	val |= 64;
	/* Set control register 2 */
	AP_ULPOSC_CON2(opp->osc) = val;
}

static void config_ulposc_cali(struct opp_ulposc_cfg *opp, uint32_t cali_val)
{
	uint32_t val;

	val = AP_ULPOSC_CON0(opp->osc);
	val &= ~OSC_CALI_MASK;
	val |= cali_val;
	AP_ULPOSC_CON0(opp->osc) = val;

	busy_udelay(50);
}

static uint32_t measure_ulposc_freq(uint32_t osc)
{
	uint32_t result = 0;
	int cnt;

	/* Before select meter clock input, bit[1:0] = b00 */
	AP_CLK_DBG_CFG = (AP_CLK_DBG_CFG & ~DBG_MODE_MASK) |
			 DBG_MODE_SET_CLOCK;

	/* Select source, bit[21:16] = clk_src */
	AP_CLK_DBG_CFG = (AP_CLK_DBG_CFG & ~DBG_BIST_SOURCE_MASK) |
			 (osc == 0 ? DBG_BIST_SOURCE_ULPOSC1 :
				     DBG_BIST_SOURCE_ULPOSC2);

	/* Set meter divisor to 1, bit[31:24] = b00000000 */
	AP_CLK_MISC_CFG_0 = (AP_CLK_MISC_CFG_0 & ~MISC_METER_DIVISOR_MASK) |
			    MISC_METER_DIV_1;

	/* Enable frequency meter, without start */
	AP_SCP_CFG_0 |= CFG_FREQ_METER_ENABLE;

	/* Trigger frequency meter start */
	AP_SCP_CFG_0 |= CFG_FREQ_METER_RUN;

	/*
	 * Frequency meter counts cycles in 1 / (26 * 1024) second period.
	 *   freq_in_hz = freq_counter * 26 * 1024
	 *
	 * The hardware takes 38us to count cycles. Delay up to 100us,
	 * as busy_udelay may not be accurate when sysclk is not 26Mhz
	 * (e.g. when recalibrating/measuring after boot).
	 */
	for (cnt = 100; cnt; cnt--) {
		busy_udelay(1);
		if (!(AP_SCP_CFG_0 & CFG_FREQ_METER_RUN)) {
			result = CFG_FREQ_COUNTER(AP_SCP_CFG_1);
			break;
		}
	}

	/* Disable freq meter */
	AP_SCP_CFG_0 &= ~CFG_FREQ_METER_ENABLE;

	return result;
}

#define CAL_MIS_RATE	40
static int is_ulposc_calibrated(struct opp_ulposc_cfg *opp)
{
	uint32_t curr, target;

	curr = measure_ulposc_freq(opp->osc);
	target = opp->target_mhz * 1024 / 26;

	/* check if calibrated value is in the range of target value +- 4% */
	if (curr > (target * (1000 - CAL_MIS_RATE) / 1000)
			&& curr < (target * (1000 + CAL_MIS_RATE) / 1000))
		return 1;
	else
		return 0;
}

static uint32_t process_ulposc_cali(struct opp_ulposc_cfg *opp)
{
	uint32_t current_val = 0;
	uint32_t target_val = opp->target_mhz * 1024 / 26;
	uint32_t middle, min = 0, max = OSC_CALI_MASK;
	uint32_t diff_by_min = 0, diff_by_max = 0xffff;
	uint32_t cal_result = 0;

	do {
		middle = (min + max) / 2;
		if (middle == min)
			break;

		config_ulposc_cali(opp, middle);
		current_val = measure_ulposc_freq(opp->osc);

		if (current_val > target_val)
			max = middle;
		else
			min = middle;
	} while (min <= max);

	config_ulposc_cali(opp, min);
	current_val = measure_ulposc_freq(opp->osc);
	if (current_val > target_val)
		diff_by_min = current_val - target_val;
	else
		diff_by_min = target_val - current_val;

	config_ulposc_cali(opp, max);
	current_val = measure_ulposc_freq(opp->osc);
	if (current_val > target_val)
		diff_by_max = current_val - target_val;
	else
		diff_by_max = target_val - current_val;

	if (diff_by_min < diff_by_max)
		cal_result = min;
	else
		cal_result = max;

	config_ulposc_cali(opp, cal_result);
	if (!is_ulposc_calibrated(opp))
		assert(0);

	return cal_result;
}

static void clock_high_enable(int osc)
{
	/* Enable high speed clock */
	SCP_CLK_EN |= EN_CLK_HIGH;

	switch (osc) {
	case 0:
		/* After 25ms, enable ULPOSC */
		busy_udelay(150);
		SCP_CLK_EN |= CG_CLK_HIGH;
		break;
	case 1:
		/* Turn off ULPOSC2 high-core-disable switch */
		SCP_CLK_ON_CTRL &= ~HIGH_CORE_DIS_SUB;
		/* After 25ms, turn on ULPOSC2 high core clock gate */
		busy_udelay(150);
		SCP_CLK_HIGH_CORE |= CLK_HIGH_CORE_CG;
		busy_udelay(50);
		break;
	default:
		break;
	}
}

static void clock_high_disable(int osc)
{
	switch (osc) {
	case 0:
		SCP_CLK_EN &= ~CG_CLK_HIGH;
		busy_udelay(50);
		SCP_CLK_EN &= ~EN_CLK_HIGH;
		busy_udelay(50);
		break;
	case 1:
		SCP_CLK_HIGH_CORE &= ~CLK_HIGH_CORE_CG;
		busy_udelay(50);
		SCP_CLK_ON_CTRL |= HIGH_CORE_DIS_SUB;
		busy_udelay(50);
		break;
	default:
		break;
	}
}

static void clock_calibrate_ulposc(struct opp_ulposc_cfg *opp)
{
	if (opp->osc != 0) {
		clock_high_disable(opp->osc);
		config_ulposc_default(opp);
		clock_high_enable(opp->osc);
	}

	if (!is_ulposc_calibrated(opp))
		opp->cali = process_ulposc_cali(opp);

	/* CPRINTF("osc:%u, target=%uMHz, cal:%u\n", opp->osc, opp->target_mhz, opp->cali); */
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

	/* Calibrate ULPOSC */
	for (int i=0; i<ARRAY_SIZE(opp); i++)
		clock_calibrate_ulposc(&opp[i]);

	/* Select ULPOSC2 high speed CPU clock */
	clock_select_clock(SCP_CLK_ULPOSC2);

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

/* Console command */
int command_ulposc(int argc, char *argv[])
{
	for (int i=0; i<=1; i++)
		CPRINTF("ULPOSC(%u) frequency: %u kHz\n", i, measure_ulposc_freq(i) * 26 * 1000 / 1024);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ulposc, command_ulposc, "[ulposc]",
                        "Measure ULPOSC frequency");
