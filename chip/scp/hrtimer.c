/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hardware timer */

#include "clock.h"
#include "common.h"
#include "hooks.h"
#include "hwtimer.h"
#include "panic.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "watchdog.h"

#define IRQ_TIMER(n) CONCAT2(SCP_IRQ_TIMER, n)

#ifndef TIMER_SYS_TICK
#define TIMER_SYS_TICK 5
#endif

#ifndef TIMER_EVENT_TICK
#define TIMER_EVENT_TICK 3
#endif

static uint32_t last_timer_event;
/* The sys clock is ticking at 26MHz. Use 40bit instead of 32bit to store */
static uint8_t sys_clock_high;
static uint8_t event_deadline_high;
static uint32_t event_deadline;

static inline void timer_set_clock(int n, uint32_t clock_source)
{
	SCP_TIMER_EN(n) = (SCP_TIMER_EN(n) & ~TIMER_CLK_MASK) |
			  clock_source;
}

static inline void timer_ack_irq(int n)
{
	SCP_TIMER_IRQ_CTRL(n) |= TIMER_IRQ_CLEAR;
}

static inline void timer_set_reset_value(int n, uint32_t reset_value)
{
	SCP_TIMER_RESET_VAL(n) = reset_value;
}

static void timer_reset(int n)
{
	__hw_timer_enable_clock(n, 0);
	timer_ack_irq(n);
	timer_set_reset_value(n, 0);
	timer_set_clock(n, TIMER_CLK_32K);
}

static void timer_reload(int n, uint32_t value)
{
	timer_set_reset_value(n, value);
	__hw_timer_enable_clock(n, 1);
}

static int event_timer_reload(void)
{
	if (event_deadline_high) {
		timer_reload(TIMER_EVENT_TICK, 0xffffffff);
		event_deadline_high--;
	} else if (event_deadline) {
		timer_reload(TIMER_EVENT_TICK, event_deadline);
		event_deadline = 0;
	}

	return event_deadline_high + !!event_deadline + 1;
}

void __hw_clock_event_clear(void)
{
	__hw_timer_enable_clock(TIMER_EVENT_TICK, 0);
	timer_set_reset_value(TIMER_EVENT_TICK, 0);
	event_deadline_high = 0;
	event_deadline = 0;
}

void __hw_clock_event_set(uint32_t deadline)
{
	uint64_t now_26m, deadline_26m;

	if (deadline == 0xffffffff) {
		__hw_clock_event_clear();
		return;
	}

	now_26m = (uint64_t)sys_clock_high << 32 |
		  (0xffffffff - SCP_TIMER_VAL(TIMER_SYS_TICK));
	deadline_26m = (uint64_t)deadline * 26;

	if (deadline_26m > now_26m) {
		deadline_26m -= now_26m;
		event_deadline_high = deadline_26m >> 32;
		event_deadline = deadline_26m & 0xffffffff;
	} else {
		event_deadline_high = 0;
		event_deadline = 1;
	}
	event_timer_reload();
}

void __hw_timer_enable_clock(int n, int enable)
{
	if (enable) {
		SCP_TIMER_IRQ_CTRL(n) |= 1;
		SCP_TIMER_EN(n) |= 1;
	} else {
		SCP_TIMER_EN(n) &= ~1;
		SCP_TIMER_IRQ_CTRL(n) &= ~1;
	}
}

int __hw_clock_source_init(uint32_t start_t)
{
	int t;

	/* Turn on timer MCLK and BCLK */
	SCP_CLK_GATE |= (CG_TIMER_M | CG_TIMER_B);

	/* Reset all timer, select 32768Hz clock source */
	for (t = 0; t < NUM_TIMERS; t++)
		timer_reset(t);

	/* Enable timer IRQ wake source */
	SCP_INTC_IRQ_WAKEUP |= (1 << IRQ_TIMER(0)) | (1 << IRQ_TIMER(1)) |
			       (1 << IRQ_TIMER(2)) | (1 << IRQ_TIMER(3)) |
			       (1 << IRQ_TIMER(4)) | (1 << IRQ_TIMER(5));
	/*
	 * Timer configuration:
	 *   OS TIMER    - count up @ 13MHz, 64bit value with latch.
	 *   SYS TICK    - count down @ 26MHz
	 *   EVENT TICK  - count down @ 26MHz
	 */

	/* Turn on OS TIMER, tick at 13MHz */
	SCP_OSTIMER_CON |= 1;

	timer_set_clock(TIMER_SYS_TICK, TIMER_CLK_26M);
	__hw_timer_enable_clock(TIMER_SYS_TICK, 1);
	timer_set_clock(TIMER_EVENT_TICK, TIMER_CLK_26M);

	return EC_SUCCESS;
}

uint32_t __hw_clock_source_read(void)
{
	uint64_t clock = ((uint64_t)sys_clock_high << 32) |
			 (0xffffffff - SCP_TIMER_VAL(TIMER_SYS_TICK));

	return (uint32_t)(clock / 26);
}

uint32_t __hw_clock_event_get(void)
{
	return last_timer_event;
}

static void __hw_clock_source_irq(int n)
{
	__hw_timer_enable_clock(n, 0);
	timer_ack_irq(n);

	switch (n) {
	case TIMER_EVENT_TICK:
		if (event_timer_reload())
			return;
		process_timers(0);
		break;
	case TIMER_SYS_TICK:
		timer_reload(TIMER_SYS_TICK, 0xffffffff);
		if (sys_clock_high < 26)
			return;
		sys_clock_high -= 26;
		process_timers(1);
		break;
	default:
		return;
	}
}

#define DECLARE_TIMER_IRQ(n) \
void __hw_clock_source_irq_##n(void) { __hw_clock_source_irq(n); } \
DECLARE_IRQ(IRQ_TIMER(n), __hw_clock_source_irq_##n, 2)

DECLARE_TIMER_IRQ(0);
DECLARE_TIMER_IRQ(1);
DECLARE_TIMER_IRQ(2);
DECLARE_TIMER_IRQ(3);
DECLARE_TIMER_IRQ(4);
DECLARE_TIMER_IRQ(5);


