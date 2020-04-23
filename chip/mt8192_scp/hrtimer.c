/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * High-res hardware timer
 *
 * SCP hardware 32bit count down timer can be configured to source clock from
 * 32KHz, 26MHz, BCLK or PCLK. This implementation selects BCLK (ULPOSC1/8) as a
 * source, countdown mode and converts to micro second value matching common
 * timer.
 */

#include "common.h"
#include "hwtimer.h"
#include "registers.h"
#include "task.h"
#include "console.h"

#define TIMER_SYSTEM 5
#define TIMER_EVENT 3
#define TIMER_CLOCK_MHZ 26
#define OVERFLOW_TICKS (26 * 0x100000000 - 1)

static uint8_t sys_high;
static uint8_t event_high;
extern volatile int ec_int;

static void timer_enable(int n)
{
	/* cannot be changed when timer is enabled */
	SCP_CORE0_TIMER_IRQ_CTRL(n) |= TIMER_IRQ_EN;
	SCP_CORE0_TIMER_EN(n) |= TIMER_EN;
}

static void timer_disable(int n)
{
	SCP_CORE0_TIMER_EN(n) &= ~TIMER_EN;
	/* cannot be changed when timer is enabled */
	SCP_CORE0_TIMER_IRQ_CTRL(n) &= ~TIMER_IRQ_EN;
}

static int timer_is_irq(int n)
{
	return SCP_CORE0_TIMER_IRQ_CTRL(n) & TIMER_IRQ_STATUS;
}

static void timer_ack_irq(int n)
{
	SCP_CORE0_TIMER_IRQ_CTRL(n) |= TIMER_IRQ_CLR;
}

static void timer_set_reset_value(int n, uint32_t reset_value)
{
	/* cannot be changed when timer is enabled */
	SCP_CORE0_TIMER_RST_VAL(n) = reset_value;
}

static void timer_set_clock(int n, uint32_t clock_source)
{
	SCP_CORE0_TIMER_EN(n) =
		(SCP_CORE0_TIMER_EN(n) & ~TIMER_CLK_SRC_MASK) | clock_source;
}

static void timer_reset(int n)
{
	timer_disable(n);
	timer_ack_irq(n);
	timer_set_reset_value(n, 0xffffffff);
	timer_set_clock(n, TIMER_CLK_SRC_32K);
}

/* Convert hardware countdown timer to 64bit countup ticks. */
static uint64_t timer_read_raw_system(void)
{
	uint32_t timer_ctrl = SCP_CORE0_TIMER_IRQ_CTRL(TIMER_SYSTEM);
	uint32_t sys_high_adj = sys_high;

	/*
	 * If an IRQ is pending, but has not been serviced yet, adjust the
	 * sys_high value.
	 */
	if (timer_ctrl & TIMER_IRQ_STATUS)
		sys_high_adj = sys_high ? (sys_high - 1)
					: (TIMER_CLOCK_MHZ - 1);

	return OVERFLOW_TICKS - (((uint64_t)sys_high_adj << 32) |
				 SCP_CORE0_TIMER_CUR_VAL(TIMER_SYSTEM));
}

static uint64_t timer_read_raw_event(void)
{
	return OVERFLOW_TICKS - (((uint64_t)event_high << 32) |
				 SCP_CORE0_TIMER_CUR_VAL(TIMER_EVENT));
}

int __hw_clock_source_init(uint32_t start_t)
{
	int t;

	/* enable clock gate */
	SCP_SET_CLK_CG |= CG_TIMER_MCLK | CG_TIMER_BCLK;

	/* reset all timer, select 32768Hz clock source */
	for (t = 0; t < NUM_TIMERS; ++t)
		timer_reset(t);

	/* enable timer IRQ wake source */

	/*
	 * Timer configuration:
	 *   OS TIMER    - count up @ 13MHz, 64bit value with latch.
	 *   SYS TICK    - count down @ 26MHz
	 *   EVENT TICK  - count down @ 26MHz
	 */

#if 0
	/* turn on OS TIMER, tick at 13MHz */
	SCP_CORE0_OS_TIMER_EN |= OS_TIMER_EN;
#endif

	/* System timestamp timer */
	timer_set_clock(TIMER_SYSTEM, TIMER_CLK_SRC_26M);
	sys_high = TIMER_CLOCK_MHZ - 1;
	timer_set_reset_value(TIMER_SYSTEM, 0xffffffff);
	task_enable_irq(SCP_IRQ_TIMER(TIMER_SYSTEM));
	timer_enable(TIMER_SYSTEM);

#if 0
	/* TODO */
	SCP_CORE0_GENERAL_CTRL |= CPU_TIMER_INT_EN;
#endif

	/* Event tick timer */
	timer_set_clock(TIMER_EVENT, TIMER_CLK_SRC_26M);
	task_enable_irq(SCP_IRQ_TIMER(TIMER_EVENT));

	return SCP_IRQ_TIMER(TIMER_SYSTEM);
}

uint32_t __hw_clock_source_read(void)
{
	return timer_read_raw_system() / TIMER_CLOCK_MHZ;
}

uint32_t __hw_clock_event_get(void)
{
	return (timer_read_raw_event() + timer_read_raw_system())
			/ TIMER_CLOCK_MHZ;
}

static void timer_reload(int n, uint32_t value)
{
	timer_disable(n);
	timer_set_reset_value(n, value);
	timer_enable(n);
}

#if 0
static int timer_reload_event_high(void)
{
	if (event_high) {
		timer_reload(TIMER_EVENT, 0xffffffff);
		event_high--;
		return 1;
	} else {
		timer_disable(TIMER_EVENT);
		return 0;
	}
}
#else
static int timer_reload_event_high(void)
{
	if (event_high) {
		if (SCP_CORE0_TIMER_RST_VAL(TIMER_EVENT) == 0xffffffff)
			timer_enable(TIMER_EVENT);
		else
			timer_reload(TIMER_EVENT, 0xffffffff);
		event_high--;
		return 1;
	}

	/* Disable event timer clock when done. */
	timer_disable(TIMER_EVENT);
	return 0;
}
#endif

void __hw_clock_event_clear(void)
{
	/* c1ea4, clear */
	timer_disable(TIMER_EVENT);
	timer_set_reset_value(TIMER_EVENT, 0x0000c1ea4);
	event_high = 0;
}

void __hw_clock_event_set(uint32_t deadline)
{
	uint64_t deadline_raw = (uint64_t)deadline * TIMER_CLOCK_MHZ;
	uint64_t now_raw = timer_read_raw_system();
	uint32_t event_deadline;

	if (deadline_raw > now_raw) {
		deadline_raw -= now_raw;
		event_deadline = (uint32_t)deadline_raw;
		event_high = deadline_raw >> 32;
	} else {
		event_deadline = 1;
		event_high = 0;
	}

	ccprints("%s: event_deadline=%x event_high=%x", __func__, event_deadline, event_high);
	cflush();

	if (event_deadline)
		timer_reload(TIMER_EVENT, event_deadline);
	else
		timer_reload_event_high();
}

#include "csr.h"
#include "timer.h"
static void irq_group6_handler(void)
{
	ccprints("%s", __func__);
	cflush();

	switch (ec_int) {
	case SCP_IRQ_TIMER(TIMER_EVENT):
#if 1
	ccprintf("mie=%x\n", (unsigned int)READ_CSR_RAW(mie));
	ccprintf("mip=%x\n", (unsigned int)READ_CSR_RAW(mip));
	ccprintf("mstatus=%x\n", (unsigned int)READ_CSR_RAW(mstatus));
	ccprintf("mcause=%x\n", (unsigned int)READ_CSR_RAW(mcause));
	ccprintf("mctren=%x\n", (unsigned int)READ_CSR(0x7c0));

	ccprintf("CSR_VIC_MICAUSE=%x\n", (unsigned int)READ_CSR(0x5c0));
	ccprintf("CSR_VIC_MIASWI=%x\n", (unsigned int)READ_CSR(0x5c1));
	ccprintf("CSR_VIC_MIEMS=%x\n", (unsigned int)READ_CSR(0x5c2));
	ccprintf("CSR_VIC_MIPEND_G0=%x\n", (unsigned int)READ_CSR(0x5d0));
	ccprintf("CSR_VIC_MIMASK_G0=%x\n", (unsigned int)READ_CSR(0x5d8));
	ccprintf("CSR_VIC_MIWAKEUP_G0=%x\n", (unsigned int)READ_CSR(0x5e0));
	ccprintf("CSR_VIC_MILSEL_G0=%x\n", (unsigned int)READ_CSR(0x5e8));
	ccprintf("CSR_VIC_MIEMASK_G0=%x\n", (unsigned int)READ_CSR(0x5f0));
	cflush();
#endif

		if (timer_is_irq(TIMER_EVENT)) {
			timer_disable(TIMER_EVENT);
			timer_ack_irq(TIMER_EVENT);
#if 1
	ccprintf("before miems mctren=%x\n", (unsigned int)READ_CSR(0x7c0));
	ccprintf("before miems: CSR_VIC_MIPEND_G0=%x\n", (unsigned int)READ_CSR(0x5d0));
	ccprintf("before miems: CORE0_TIMER_IRQ_CTRL(timer3)=%x\n", SCP_CORE0_TIMER_IRQ_CTRL(TIMER_EVENT));
	ccprintf("before miems: CORE0_TIMER_CUR_VAL(timer3)=%x\n", SCP_CORE0_TIMER_CUR_VAL(TIMER_EVENT));
	ccprintf("before miems: CORE0_INTC_IRQ_OUT=%x\n", SCP_CORE0_INTC_IRQ_OUT);
	ccprintf("before miems: CORE0_INTC_IRQ_STA0=%x\n", REG32(0x70032010));
	ccprintf("before miems: CORE0_INTC_IRQ_GRP6_STA0=%x\n", SCP_CORE0_INTC_IRQ_GRP_STA(6, 0));
	cflush();
#endif
			task_clear_pending_irq(ec_int);
			if (timer_reload_event_high())
				return;
#if 1
	ccprintf("after miems mctren=%x\n", (unsigned int)READ_CSR(0x7c0));
	ccprintf("after miems: CSR_VIC_MIPEND_G0=%x\n", (unsigned int)READ_CSR(0x5d0));
	ccprintf("after miems: CORE0_TIMER_IRQ_CTRL(timer3)=%x\n", SCP_CORE0_TIMER_IRQ_CTRL(TIMER_EVENT));
	ccprintf("after miems: CORE0_TIMER_CUR_VAL(timer3)=%x\n", SCP_CORE0_TIMER_CUR_VAL(TIMER_EVENT));
	ccprintf("after miems: CORE0_INTC_IRQ_OUT=%x\n", SCP_CORE0_INTC_IRQ_OUT);
	ccprintf("after miems: CORE0_INTC_IRQ_STA0=%x\n", REG32(0x70032010));
	ccprintf("after miems: CORE0_INTC_IRQ_GRP6_STA0=%x\n", SCP_CORE0_INTC_IRQ_GRP_STA(6, 0));
	cflush();
#endif

		}
		process_timers(0);
#if 1
	ccprintf("before exiting: mctren=%x\n", (unsigned int)READ_CSR(0x7c0));
	ccprintf("before exiting: CSR_VIC_MIPEND_G0=%x\n", (unsigned int)READ_CSR(0x5d0));
	ccprintf("before exiting: CORE0_TIMER_IRQ_CTRL(timer3)=%x\n", SCP_CORE0_TIMER_IRQ_CTRL(TIMER_EVENT));
	ccprintf("before exiting: CORE0_TIMER_CUR_VAL(timer3)=%x\n", SCP_CORE0_TIMER_CUR_VAL(TIMER_EVENT));
	ccprintf("before exiting: CORE0_INTC_IRQ_OUT=%x\n", SCP_CORE0_INTC_IRQ_OUT);
	ccprintf("before exiting: CORE0_INTC_IRQ_STA0=%x\n", REG32(0x70032010));
	ccprintf("before exiting: CORE0_INTC_IRQ_GRP6_STA0=%x\n", SCP_CORE0_INTC_IRQ_GRP_STA(6, 0));
	cflush();
#endif

		break;
	case SCP_IRQ_TIMER(TIMER_SYSTEM):
		/* If this is a hardware irq, check overflow */
		if (timer_is_irq(TIMER_SYSTEM)) {
			timer_ack_irq(TIMER_SYSTEM);
			task_clear_pending_irq(ec_int);
			if (sys_high) {
				--sys_high;
				process_timers(0);
			} else {
				/* Overflow, reload system timer */
				sys_high = TIMER_CLOCK_MHZ - 1;
				process_timers(1);
			}
		} else {
			process_timers(0);
		}
		break;
	}
}
DECLARE_IRQ(6, irq_group6_handler, 2);
