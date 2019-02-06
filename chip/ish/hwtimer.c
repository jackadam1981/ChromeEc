/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hardware timers driver  - HPET */

#include "console.h"
#include "hpet.h"
#include "hwtimer.h"
#include "timer.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#if defined(CHIP_FAMILY_ISH3)
#define CLOCK_FACTOR 12
#endif

#define CPUTS(outstr) cputs(CC_CLOCK, outstr)
#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CLOCK, format, ## args)

static uint32_t last_deadline;

#if defined(CHIP_FAMILY_ISH4) || defined(CHIP_FAMILY_ISH5)
/*
 * For ISH variants with 32kHz timers, we need to keep track of the most
 * recent rollover (2 ^ 32 us) comparison. This ensure that our
 * __hw_clock_source_read method is in sync with the rollover mechanism.
 * Without this cache, the value returned from __hw_clock_source_read could
 * potentially return a high number (e.g. 0xFFFFFFF7) after the rollover IRQ has
 * fired.
 */
static uint32_t last_rollover_cmp;
static uint32_t cached_rollover_cmp;
#endif

/*
 * The ISH hardware needs at least 25 ticks of leeway to arms the timer.
 * ISH4/5 are the slowest with 32kHz timers, so we wait at least 800us when
 * scheduling events in the future
 */
#define MINIMUM_EVENT_DELAY_US 800

/* Helper methods for different ISH chip variants */
#ifdef CHIP_FAMILY_ISH3
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
#endif /* CHIP_FAMILY_ISH3 */

/* Helper methods for different ISH chip variants */
#if defined(CHIP_FAMILY_ISH4) || defined(CHIP_FAMILY_ISH5)
#define CLOCK_SCALE_BITS 15
BUILD_ASSERT(ISH_HPET_CLK_FREQ == (1 << CLOCK_SCALE_BITS));

static inline uint32_t scale_us2ticks(uint32_t us)
{
	/*
	 * ticks = us / SECOND * ISH_HPET_CLK_FREQ;
	 *
	 * First multiple us by ISH_HPET_CLK_FREQ via bit shift, then use
	 * 64-bit div into 32-bit result.
	 */
	const uint32_t hi = us >> (32 - CLOCK_SCALE_BITS);
	const uint32_t lo = us << CLOCK_SCALE_BITS;
	const uint32_t divisor = SECOND;
	uint32_t ticks;

	asm("divl %3" : "=a"(ticks) : "d"(hi), "a"(lo), "rm"(divisor));
	return ticks;
}

static inline uint32_t scale_ticks2us(uint32_t ticks)
{
	/*
	 * us = ticks / ISH_HPET_CLK_FREQ * SECOND;
	 */
	const uint64_t intermediate = (uint64_t)ticks * SECOND;

	return intermediate >> CLOCK_SCALE_BITS;
}
#endif /* CHIP_FAMILY_ISH4 || CHIP_FAMILY_ISH5 */

void __hw_clock_event_set(uint32_t deadline)
{
	uint32_t remaining_us;

	last_deadline = deadline;

	remaining_us = deadline - __hw_clock_source_read();

	/* Ensure HW has enough time to react to new timer value */
	remaining_us = MAX(remaining_us, MINIMUM_EVENT_DELAY_US);

#if defined(CHIP_FAMILY_ISH3)
	/*
	 * This assumes that remaining_us is less than 360 seconds (2^32 us /
	 * 12Mhz), otherwise we would need to handle 32-bit rollover of 12Mhz
	 * timer comparator value. Watchdog refresh happens at least every 10
	 * seconds.
	 */
	HPET_TIMER_COMP(1) = deadline * CLOCK_FACTOR;
#elif defined(CHIP_FAMILY_ISH4) || defined(CHIP_FAMILY_ISH5)
	/*
	 * This assumes that remaining_us is less than 16 seconds since the
	 * complier optimization to scale will drop the top 7 bits of remaining
	 * us to perform the calculation without a div operation. This is okay
	 * since the watchdog refresh happens at least every 10 seconds.
	 */
	HPET_TIMER_COMP(1) =
		HPET_MAIN_COUNTER + (remaining_us * ISH_HPET_CLK_FREQ / SECOND);
#endif
	/* Arm timer */
	HPET_TIMER_CONF_CAP(1) |= HPET_Tn_INT_ENB_CNF;

#if defined(CHIP_FAMILY_ISH4) || defined(CHIP_FAMILY_ISH5)
	/* Wait for timer to settle */
	while (HPET_CTRL_STATUS & HPET_T1_SETTLING)
		;
#endif
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
#if defined(CHIP_FAMILY_ISH3)
	const uint64_t tmp = read_main_timer();
	const uint32_t divisor = CLOCK_FACTOR;
	/*
	 * Modulating hi first ensures that the quotient fits in 32-bits due to
	 * the follow math:
	 * Let tmp = (hi << 32) + lo;
	 * Let hi = N*CLOCK_FACTOR + R; where R is hi % CLOCK_FACTOR
	 *
	 * tmp = (N*CLOCK_FACTOR << 32) + (R << 32) + lo
	 *
	 * tmp / CLOCK_FACTOR = ((N*CLOCK_FACTOR << 32) + (R << 32) + lo) /
	 *                      CLOCK_FACTOR
	 * tmp / CLOCK_FACTOR = (N*CLOCK_FACTOR << 32) / CLOCK_FACTOR +
	 *                      (R << 32) / CLOCK_FACTOR +
	 *                      lo / CLOCK_FACTOR
	 * tmp / CLOCK_FACTOR = (N << 32) +
	 *                      (R << 32) / CLOCK_FACTOR +
	 *                      lo / CLOCK_FACTOR
	 * If we want to truncate to 32 bits, then the N << 32 can be dropped.
	 * (tmp / CLOCK_FACTOR) & 0xFFFFFFFF = ((R << 32) + lo) / CLOCK_FACTOR
	 */
	const uint32_t hi = ((uint32_t)(tmp >> 32)) % divisor;
	const uint32_t lo = tmp;

	register uint32_t quotient;
	asm("divl %3" : "=a"(quotient) : "d"(hi), "a"(lo), "rm"(divisor));
	return quotient;
#else
	return scale_ticks2us(HPET_MAIN_COUNTER - last_rollover_cmp);
#endif
}

void __hw_clock_source_set(uint32_t ts)
{
	/* Reset both clock and overflow comparators */

	HPET_GENERAL_CONFIG &= ~HPET_ENABLE_CNF;

#if defined(CHIP_FAMILY_ISH3)
	HPET_MAIN_COUNTER_64 = (uint64_t)ts * CLOCK_FACTOR;
	HPET_TIMER_COMP_64(0) = (uint64_t)CLOCK_FACTOR << 32;
#else
	HPET_MAIN_COUNTER = scale_us2ticks(ts);
	HPET_TIMER_COMP(0) = ((uint64_t)ISH_HPET_CLK_FREQ << 32) / SECOND;
#endif

	HPET_GENERAL_CONFIG |= HPET_ENABLE_CNF;
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
#if defined(CHIP_FAMILY_ISH4) || defined(CHIP_FAMILY_ISH5)
	/*
	 * Cache the most recent comparator value for __hw_clock_source_read.
	 * HPET_TIMER_COMP(0) has already been updated to the new value by the
	 * time this IRQ is run.
	 */
	last_rollover_cmp = cached_rollover_cmp;
	cached_rollover_cmp = HPET_TIMER_COMP(0);
#endif
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
#if defined(CHIP_FAMILY_ISH3)
	HPET_MAIN_COUNTER_64 = (uint64_t)start_t * CLOCK_FACTOR;
#else
	HPET_MAIN_COUNTER = scale_us2ticks(start_t);
#endif

#if defined(CHIP_FAMILY_ISH3)
	/*
	 * Set comparator value. HMC will operate in 64 bit mode.
	 * HMC is 12MHz, Hence set COMP to 12x of 1MHz.
	 */
	HPET_TIMER_COMP_64(0) = (uint64_t)CLOCK_FACTOR << 32;
	/* TIMER0 in 64-bit mode */
	timer0_config &= ~HPET_Tn_32MODE_CNF;
#else
	/* Set comparator value - We will lose ~10.8us every 1.2 hours */
	HPET_TIMER_COMP(0) = ((uint64_t)ISH_HPET_CLK_FREQ << 32) / SECOND;
	/*TIMER0 in 32-bit mode*/
	timer0_config |= HPET_Tn_32MODE_CNF;
#endif

	/* Timer 0 - Enable periodic mode for rollover */
	timer0_config |= HPET_Tn_TYPE_CNF;

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

	/* Unmask HPET IRQ in IOAPIC */
	task_enable_irq(ISH_HPET_TIMER0_IRQ);
	task_enable_irq(ISH_HPET_TIMER1_IRQ);

	/* Set timer 0/1 config */
	HPET_TIMER_CONF_CAP(0) |= timer0_config;
	HPET_TIMER_CONF_CAP(1) |= timer1_config;

#if defined(CHIP_FAMILY_ISH4) || defined(CHIP_FAMILY_ISH5)
	/* Wait for timer to settle. required for ISH 4 */
	while (HPET_CTRL_STATUS & HPET_MAIN_COUNTER_SETTLING)
		;
#endif

	/*
	 * LEGACY_RT_CNF for HPET1 interrupt routing
	 * and enable overall HPET counter/interrupts.
	 */
	HPET_GENERAL_CONFIG |= (HPET_ENABLE_CNF | HPET_LEGACY_RT_CNF);

	return ISH_HPET_TIMER1_IRQ;
}
