/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hardware timers driver  - HPET */

#include "clock.h"
#include "common.h"
#include "hooks.h"
#include "hwtimer.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "interrupts.h"
#include "hpet.h"
#include "console.h"

#define CPUTS(outstr) cputs(CC_CLOCK, outstr)
#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CLOCK, format, ## args)

void __hw_clock_event_set(uint32_t deadline)
{
	HPET_TIMER_COMP(1) = REG32(HPET_BASE + MAIN_COUNTER_REG) + deadline;
	HPET_TIMER_CONF_CAP(1) |= HPET_Tn_INT_ENB_CNF;
}

inline uint64_t get_tsc(void)
{
	uint32_t hi, lo;

	__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
	return (uint64_t)hi << 32 | lo;
}

uint32_t __hw_clock_event_get(void)
{
	return 0;
}

void __hw_clock_event_clear(void)
{
	HPET_TIMER_CONF_CAP(1) &= ~HPET_Tn_INT_ENB_CNF;
}

uint32_t __hw_clock_source_read(void)
{
	return REG32(HPET_BASE + MAIN_COUNTER_REG);
}

void __hw_clock_source_set(uint32_t ts)
{
}

static void __hw_clock_source_irq(int timer_id)
{
	/* Clear interrupt */
	HPET_INTR_CLEAR = (1 << timer_id);

	/* If IRQ is from timer 0, 32-bit timer overflowed */
	process_timers(timer_id == 0);
}

void __hw_clock_source_irq_0(void)
{
	__hw_clock_source_irq(0);
}

DECLARE_IRQ(ISH30_HPET_TIMER0_IRQ, __hw_clock_source_irq_0);

void __hw_clock_source_irq_1(void)
{
	__hw_clock_source_irq(1);
}

DECLARE_IRQ(ISH30_HPET_TIMER1_IRQ, __hw_clock_source_irq_1);

int __hw_clock_source_init(uint32_t start_t)
{

	/*
	 * The timer can only fire interrupt when its value reaches zero.
	 * Therefore we need two timers:
	 *   - Timer 0 as free running timer
	 *   - Timer 1 as event timer
	 */

	/* Disable HPET */
	HPET_GENERAL_CONFIG &= ~HPET_ENABLE_CNF;
	HPET_MAIN_COUNTER = 0;

	/* Set comparator value */
	HPET_TIMER_COMP(0) = 0x1f4240 - start_t;

	/* Timer 0 - periodic */
	HPET_TIMER_CONF_CAP(0) |= HPET_Tn_TYPE_CNF;
	HPET_TIMER_CONF_CAP(0) |= HPET_Tn_32MODE_CNF;

	/* Set IRQ */
#if ISH30_HPET_TIMER0_IRQ < 32
	HPET_TIMER_CONF_CAP(0) &= ~HPET_Tn_INT_ROUTE_CNF_MASK;
	HPET_TIMER_CONF_CAP(0) |= (ISH30_HPET_TIMER0_IRQ <<
			HPET_Tn_INT_ROUTE_CNF_SHIFT);
#else
	HPET_TIMER_CONF_CAP(0) &= ~HPET_Tn_INT_ROUTE_CNF_MASK;
#endif

	/* Level interrupt */
	HPET_TIMER_CONF_CAP(0) |= HPET_Tn_INT_TYPE_CNF;
	HPET_TIMER_CONF_CAP(1) |= HPET_Tn_INT_TYPE_CNF;

	/* Unask IRQ in IOAPIC */
	task_enable_irq(ISH30_HPET_TIMER0_IRQ);
	task_enable_irq(ISH30_HPET_TIMER1_IRQ);

	/* Enable interrupt */
	HPET_TIMER_CONF_CAP(0)	|= HPET_Tn_INT_ENB_CNF;
	HPET_TIMER_CONF_CAP(1)	|= HPET_Tn_INT_ENB_CNF;

	/* Enable HPET counter */
	HPET_GENERAL_CONFIG |= HPET_ENABLE_CNF | HPET_LEGACY_RT_CNF;

	return ISH30_HPET_TIMER1_IRQ; /* One shot */
}

