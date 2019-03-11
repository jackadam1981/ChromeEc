/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hardware timers driver  - HPET */

#include "console.h"
#include "hpet.h"
#include "hwtimer.h"
#include "registers.h"
#include "task.h"

#define CPUTS(outstr) cputs(CC_CLOCK, outstr)
#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CLOCK, format, ## args)

static uint32_t last_deadline;

/* TODO: Conform to EC API
 * ISH supports 32KHz and 12MHz clock sources.
 * EC expects timer value in 1MHz.
 * Scale the values and support it.
 */
#define ROLLOVER_CMP_VAL (((uint64_t)ISH_HPET_CLK_FREQ << 32) / SECOND)

/*
 * The ISH hardware needs at least 25 ticks of leeway to arms the timer.
 * ISH4/5 are the slowest with 32kHz timers, so we wait at least 800us when
 * scheduling events in the future
 */
#define MINIMUM_EVENT_DELAY_US 800

/* Scaling helper methods for different ISH chip variants */
#ifdef CHIP_FAMILY_ISH3
#define CLOCK_FACTOR 12
BUILD_ASSERT(CLOCK_FACTOR * SECOND == ISH_HPET_CLK_FREQ);

static inline uint64_t scale_us2ticks(uint32_t us)
{
	return (uint64_t)us * CLOCK_FACTOR;
}

static inline uint32_t scale_ticks2us(uint64_t ticks)
{
	/*
	 * We drop into asm here since this is on the critical path of reading
	 * the hardware timer for clock result. This needs to be efficient.
	 *
	 * Modulating hi first ensures that the quotient fits in 32-bits due to
	 * the follow math:
	 * Let ticks = (hi << 32) + lo;
	 * Let hi = N*CLOCK_FACTOR + R; where R is hi % CLOCK_FACTOR
	 *
	 * ticks = (N*CLOCK_FACTOR << 32) + (R << 32) + lo
	 *
	 * ticks / CLOCK_FACTOR = ((N*CLOCK_FACTOR << 32) + (R << 32) + lo) /
	 *                        CLOCK_FACTOR
	 * ticks / CLOCK_FACTOR = (N*CLOCK_FACTOR << 32) / CLOCK_FACTOR +
	 *                        (R << 32) / CLOCK_FACTOR +
	 *                        lo / CLOCK_FACTOR
	 * ticks / CLOCK_FACTOR = (N << 32) +
	 *                        (R << 32) / CLOCK_FACTOR +
	 *                        lo / CLOCK_FACTOR
	 * If we want to truncate to 32 bits, then the N << 32 can be dropped.
	 * (ticks / CLOCK_FACTOR) & 0xFFFFFFFF = ((R << 32) + lo) / CLOCK_FACTOR
	 */
	const uint32_t divisor = CLOCK_FACTOR;
	const uint32_t hi = ((uint32_t)(ticks >> 32)) % divisor;
	const uint32_t lo = ticks;
	uint32_t quotient;

	asm("divl %3" : "=a"(quotient) : "d"(hi), "a"(lo), "rm"(divisor));
	return quotient;
}

#elif defined(CHIP_FAMILY_ISH4) || defined(CHIP_FAMILY_ISH5)
#define CLOCK_SCALE_BITS 15
BUILD_ASSERT(BIT(CLOCK_SCALE_BITS) == ISH_HPET_CLK_FREQ);

static inline uint32_t scale_us2ticks(uint32_t us)
{
	/*
	 * ticks = us * ISH_HPET_CLK_FREQ / SECOND;
	 *
	 * First multiple us by ISH_HPET_CLK_FREQ via bit shift, then use
	 * 64-bit div into 32-bit result.
	 *
	 * We use asm directly to maintain full 32-bit precision without using
	 * an iterative divide (i.e. 64-bit / 64-bit => 64-bit). We use the
	 * 64-bit / 32-bit => 32-bit asm instruction directly since there is no
	 * way to emitted that instruction via the compiler.
	 *
	 * The intermediate result of (us * ISH_HPET_CLK_FREQ) needs 64-bits of
	 * precision to maintain full 32-bit precision for the end result.
	 */
	const uint32_t hi = us >> (32 - CLOCK_SCALE_BITS);
	const uint32_t lo = us << CLOCK_SCALE_BITS;
	const uint32_t divisor = SECOND;
	uint32_t ticks;

	asm("divl %3" : "=a"(ticks) : "d"(hi), "a"(lo), "rm"(divisor));
	return ticks;
}

static inline uint32_t scale_ticks2us(uint64_t ticks)
{
	/*
	 * us = ticks / ISH_HPET_CLK_FREQ * SECOND;
	 */
	const uint64_t intermediate = (uint64_t)ticks * SECOND;

	return intermediate >> CLOCK_SCALE_BITS;
}
#endif /* CHIP_FAMILY_ISH4 || CHIP_FAMILY_ISH5 */

/*
 * The 64-bit read on a 32-bit chip can tear during the read. Ensure that the
 * value returned for 64-bit didn't rollover while we were reading it.
 */
static inline uint64_t read_main_timer(void)
{
	timestamp_t t;
	uint32_t hi;

	do {
		t.le.hi = HPET_MAIN_COUNTER_64_HI;
		t.le.lo = HPET_MAIN_COUNTER_64_LO;
		hi = HPET_MAIN_COUNTER_64_HI;
	} while (t.le.hi != hi);

	return t.val;
}

void __hw_clock_event_set(uint32_t deadline)
{
	last_deadline = deadline;
	HPET_TIMER_COMP(1) = deadline;
	HPET_TIMER_CONF_CAP(1) |= HPET_Tn_INT_ENB_CNF;
}

uint32_t __hw_clock_event_get(void)
{
	return last_deadline;
}

void __hw_clock_event_clear(void)
{
	HPET_TIMER_CONF_CAP(1) &= ~HPET_Tn_INT_ENB_CNF;
}

uint32_t __hw_clock_source_read(void)
{
	return HPET_MAIN_COUNTER;
}

void __hw_clock_source_set(uint32_t ts)
{
	HPET_GENERAL_CONFIG &= ~HPET_ENABLE_CNF;
	HPET_MAIN_COUNTER = ts;
	HPET_GENERAL_CONFIG |= HPET_ENABLE_CNF;
}

static void __hw_clock_source_irq(int timer_id)
{
	/* Clear interrupt */
	HPET_INTR_CLEAR = BIT(timer_id);

	/* If IRQ is from timer 0, 32-bit timer overflowed */
	process_timers(timer_id == 0);
}

void __hw_clock_source_irq_0(void)
{
	__hw_clock_source_irq(0);
}
DECLARE_IRQ(ISH_HPET_TIMER0_IRQ, __hw_clock_source_irq_0);

void __hw_clock_source_irq_1(void)
{
	__hw_clock_source_irq(1);
}
DECLARE_IRQ(ISH_HPET_TIMER1_IRQ, __hw_clock_source_irq_1);

int __hw_clock_source_init(uint32_t start_t)
{

	/*
	 * The timer can only fire interrupt when its value reaches zero.
	 * Therefore we need two timers:
	 *   - Timer 0 as free running timer
	 *   - Timer 1 as event timer
	 */

	uint32_t timer0_config = 0x00000000;
	uint32_t timer1_config = 0x00000000;

	/* Disable HPET */
	HPET_GENERAL_CONFIG &= ~HPET_ENABLE_CNF;
	HPET_MAIN_COUNTER = start_t;

	/* Set comparator value */
	HPET_TIMER_COMP(0) = 0XFFFFFFFF;

	/* Timer 0 - enable periodic mode */
	timer0_config |= HPET_Tn_TYPE_CNF;
	timer0_config |= HPET_Tn_32MODE_CNF;
	timer0_config |= HPET_Tn_VAL_SET_CNF;

	/* Timer 0 - IRQ routing, no need IRQ set for HPET0 */
	timer0_config &= ~HPET_Tn_INT_ROUTE_CNF_MASK;

	/* Timer 1 - IRQ routing */
	timer1_config &= ~HPET_Tn_INT_ROUTE_CNF_MASK;
	timer1_config |= (ISH_HPET_TIMER1_IRQ <<
				HPET_Tn_INT_ROUTE_CNF_SHIFT);

	/* Level triggered interrupt */
	timer0_config |= HPET_Tn_INT_TYPE_CNF;
	timer1_config |= HPET_Tn_INT_TYPE_CNF;

	/* Enable interrupt */
	timer0_config |= HPET_Tn_INT_ENB_CNF;
	timer1_config |= HPET_Tn_INT_ENB_CNF;

	/* Unask HPET IRQ in IOAPIC */
	task_enable_irq(ISH_HPET_TIMER0_IRQ);
	task_enable_irq(ISH_HPET_TIMER1_IRQ);

	/* Set timer 0/1 config */
	HPET_TIMER_CONF_CAP(0) |= timer0_config;
	HPET_TIMER_CONF_CAP(1) |= timer1_config;

#if defined CONFIG_ISH_40
	/* Wait for timer to settle. required for ISH 4 */
	while (HPET_CTRL_STATUS & HPET_T_CONF_CAP_BIT)
		;
#endif

	/*
	 * LEGACY_RT_CNF for HPET1 interrupt routing
	 * and enable overall HPET counter/interrupts.
	 */
	HPET_GENERAL_CONFIG |= (HPET_ENABLE_CNF | HPET_LEGACY_RT_CNF);

	return ISH_HPET_TIMER1_IRQ;
}
