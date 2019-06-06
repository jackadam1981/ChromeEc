/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 Clocks and Power Management Module for Chrome EC */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"
#include "watchdog.h"
#include "registers.h"
#include "gcr_regs.h"
#include "pwrseq_regs.h"
#include "tmr_regs.h"
#include "wdt_regs.h"
#include "mxc_errors.h"

#define MAX32660_SYSTEMCLOCK SYS_CLOCK_HIRC

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CLOCK, outstr)
#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ##args)

/***** Definitions *****/
#define SYS_CLOCK_TIMEOUT MXC_DELAY_MSEC(1)

#define SYS_RTC_CLK 32768UL

/** @brief Clock source */
typedef enum {
	SYS_CLOCK_NANORING = MXC_V_GCR_CLKCN_CLKSEL_NANORING, /**< 8KHz nanoring
								 on MAX32660 */
	SYS_CLOCK_HFXIN =
		MXC_V_GCR_CLKCN_CLKSEL_HFXIN, /**< 32KHz on MAX32660 */
	SYS_CLOCK_HFXIN_DIGITAL = 0x9,	/**< External Clock Input*/
	SYS_CLOCK_HIRC = MXC_V_GCR_CLKCN_CLKSEL_HIRC, /**< High Frequency
							 Internal Oscillator */
} sys_system_clock_t;

/***** Functions ******/
static int clock_timeout(uint32_t ready)
{
	// Start timeout, wait for ready
	//	mxc_delay_start(SYS_CLOCK_TIMEOUT);
	do {
		if (MXC_GCR->clkcn & ready) {
			return E_NO_ERROR;
		}
	} while (1);
	//	} while (mxc_delay_check() == E_NO_ERROR);

	return E_NO_ERROR;
}

extern void (*const __isr_vector[])(void);
uint32_t SystemCoreClock = HIRC96_FREQ;

void clock_update(void)
{
	uint32_t base_freq, divide, ovr;

	// Get the clock source and frequency
	ovr = (MXC_PWRSEQ->lp_ctrl & MXC_F_PWRSEQ_LP_CTRL_OVR);
	if (ovr == MXC_S_PWRSEQ_LP_CTRL_OVR_0_9V) {
		base_freq = HIRC96_FREQ / 4;
	} else {
		if (ovr == MXC_S_PWRSEQ_LP_CTRL_OVR_1_0V) {
			base_freq = HIRC96_FREQ / 2;
		} else {
			base_freq = HIRC96_FREQ;
		}
	}

	// Get the clock divider
	divide = (MXC_GCR->clkcn & MXC_F_GCR_CLKCN_PSC) >>
		 MXC_F_GCR_CLKCN_PSC_POS;

	SystemCoreClock = base_freq >> divide;
}

int clock_select(sys_system_clock_t clock, mxc_tmr_regs_t *tmr)
{
	uint32_t current_clock, ovr, divide;

	// Save the current system clock
	current_clock = MXC_GCR->clkcn & MXC_F_GCR_CLKCN_CLKSEL;
	// Set FWS higher than what the minimum for the fastest clock is
	MXC_GCR->memckcn = (MXC_GCR->memckcn & ~(MXC_F_GCR_MEMCKCN_FWS)) |
			   (0x5UL << MXC_F_GCR_MEMCKCN_FWS_POS);
	switch (clock) {

	case SYS_CLOCK_HIRC:
		// Enable 96MHz Clock
		MXC_GCR->clkcn |= MXC_F_GCR_CLKCN_HIRC_EN;

		// Check if 96MHz clock is ready
		if (clock_timeout(MXC_F_GCR_CLKCN_HIRC_RDY) != E_NO_ERROR) {
			return E_TIME_OUT;
		}

		// Set 96MHz clock as System Clock
		MXC_SETFIELD(MXC_GCR->clkcn, MXC_F_GCR_CLKCN_CLKSEL,
			     MXC_S_GCR_CLKCN_CLKSEL_HIRC);

		break;
	default:
		return E_BAD_PARAM;
	}

	// Wait for system clock to be ready
	if (clock_timeout(MXC_F_GCR_CLKCN_CKRDY) != E_NO_ERROR) {

		// Restore the old system clock if timeout
		MXC_SETFIELD(MXC_GCR->clkcn, MXC_F_GCR_CLKCN_CLKSEL,
			     current_clock);

		return E_TIME_OUT;
	}

	// Update the system core clock
	clock_update();

	// Get the clock divider
	divide = (MXC_GCR->clkcn & MXC_F_GCR_CLKCN_PSC) >>
		 MXC_F_GCR_CLKCN_PSC_POS;

	// get ovr setting
	ovr = (MXC_PWRSEQ->lp_ctrl & MXC_F_PWRSEQ_LP_CTRL_OVR);

	// Set flash wait settings
	if (ovr == MXC_S_PWRSEQ_LP_CTRL_OVR_0_9V) {

		if (divide == 0) {
			MXC_GCR->memckcn =
				(MXC_GCR->memckcn & ~(MXC_F_GCR_MEMCKCN_FWS)) |
				(0x2UL << MXC_F_GCR_MEMCKCN_FWS_POS);

		} else {
			MXC_GCR->memckcn =
				(MXC_GCR->memckcn & ~(MXC_F_GCR_MEMCKCN_FWS)) |
				(0x1UL << MXC_F_GCR_MEMCKCN_FWS_POS);
		}

	} else if (ovr == MXC_S_PWRSEQ_LP_CTRL_OVR_1_0V) {
		if (divide == 0) {
			MXC_GCR->memckcn =
				(MXC_GCR->memckcn & ~(MXC_F_GCR_MEMCKCN_FWS)) |
				(0x2UL << MXC_F_GCR_MEMCKCN_FWS_POS);

		} else {
			MXC_GCR->memckcn =
				(MXC_GCR->memckcn & ~(MXC_F_GCR_MEMCKCN_FWS)) |
				(0x1UL << MXC_F_GCR_MEMCKCN_FWS_POS);
		}

	} else {

		if (divide == 0) {
			MXC_GCR->memckcn =
				(MXC_GCR->memckcn & ~(MXC_F_GCR_MEMCKCN_FWS)) |
				(0x4UL << MXC_F_GCR_MEMCKCN_FWS_POS);

		} else if (divide == 1) {
			MXC_GCR->memckcn =
				(MXC_GCR->memckcn & ~(MXC_F_GCR_MEMCKCN_FWS)) |
				(0x2UL << MXC_F_GCR_MEMCKCN_FWS_POS);

		} else {
			MXC_GCR->memckcn =
				(MXC_GCR->memckcn & ~(MXC_F_GCR_MEMCKCN_FWS)) |
				(0x1UL << MXC_F_GCR_MEMCKCN_FWS_POS);
		}
	}

	return E_NO_ERROR;
}

void clock_init(void)
{
	/* Switch system clock to HIRC */
	clock_select(MAX32660_SYSTEMCLOCK, MXC_TMR0);
}
