/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hardware timers driver */

#include "clock.h"
#include "common.h"
#include "hooks.h"
#include "hwtimer.h"
#include "registers.h"
#include "task.h"
#include "timer.h"

/* Using TIMER0 */
#define TIMER LM3_TIMER_BASE(0)

void __hw_clock_event_set(uint32_t deadline)
{
	/* set the match on the deadline */
	LM3_TIMER_TAMATCHR(6) = 0xffffffff - deadline;
	/* Set the match interrupt */
	LM3_TIMER_IMR(TIMER) |= LM3_TIMER_IMR_CAMIM;
}

uint32_t __hw_clock_event_get(void)
{
	return 0xffffffff - LM3_TIMER_TAMATCHR(6);
	return 0;
}

void __hw_clock_event_clear(void)
{
	/* Disable the match interrupt */
	LM3_TIMER_IMR(TIMER) &= ~LM3_TIMER_IMR_CAMIM;
}

uint32_t __hw_clock_source_read(void)
{
	return 0xffffffff - LM3_TIMER_TAILR(TIMER);
	return 0;
}

void __hw_clock_source_set(uint32_t ts)
{
	LM3_TIMER_TAILR(TIMER) = 0xffffffff - ts;
}

void __hw_clock_source_irq(void)
{
	uint32_t status = LM3_TIMER_RIS(TIMER);

	/* Clear interrupt */
	LM3_TIMER_ICR(TIMER) = status;

	/*
	 * Find expired timers and set the new timer deadline; check the IRQ
	 * status to determine if the free-running counter overflowed.
	 */
	process_timers(status & 0x01);
}
DECLARE_IRQ(LM3_IRQ_TIMER0A, __hw_clock_source_irq, 1);

static void update_prescaler(void)
{
	/*
	 * Set the prescaler to increment every microsecond.  This takes
	 * effect immediately, because the TAILD bit in TAMR is clear.
	 */
	LM3_TIMER_TAPR(TIMER) = clock_get_freq() / SECOND;
}
DECLARE_HOOK(HOOK_FREQ_CHANGE, update_prescaler, HOOK_PRIO_DEFAULT);

int __hw_clock_source_init(uint32_t start_t)
{
	/*
	 * Use TIMER0 configured as a free running counter with 1 us
	 * period.
	 */

	/* Enable TIMER0 clock in run and sleep modes. */
	clock_enable_peripheral(CGC_OFFSET_TIMER0, 0x1,
			CGC_MODE_RUN | CGC_MODE_SLEEP);

	/* Ensure timer is disabled : TAEN = 0 */
	LM3_TIMER_CTL(TIMER) &= ~LM3_TIMER_CTL_TAEN;

	/*
	 * Write the GPTM Configuration Register (GPTMCFG)
	 * with a value of 0x0
	 */
	LM3_TIMER_CFG(TIMER) = 0;
	/* Select Periodic mode */
	LM3_TIMER_TAMR(TIMER) |= LM3_TIMER_TAMR_TAMR(2);
	/* Use the full 32-bits of the timer */
	LM3_TIMER_TAILR(TIMER) = 0xffffffff;
	/* Set overflow interrupt */
	LM3_TIMER_IMR(TIMER) |= LM3_TIMER_IMR_TATOIM;

	/* Set initial prescaler */
	update_prescaler();

	/* Starts counting in timer */
	LM3_TIMER_CTL(TIMER) |= LM3_TIMER_CTL_TAEN;

	/*
	 * Override the count with the start value now that counting has
	 * started.
	 */
	__hw_clock_source_set(start_t);

	/* Enable interrupt */
	task_enable_irq(LM3_IRQ_TIMER0A);
	return LM3_IRQ_TIMER0A;
}
