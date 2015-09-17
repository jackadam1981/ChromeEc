/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hardware timers driver */

#include "cpu.h"
#include "common.h"
#include "hooks.h"
#include "hwtimer.h"
#include "hwtimer_chip.h"
#include "irq_chip.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

#define TIMER_COUNT_1US_SHIFT      3

#define MS_TO_COUNT(hz, ms) ((hz) * (ms) / 1000)

const struct ext_timer_ctrl_t et_ctrl_regs[] = {
	{&IT83XX_INTC_IELMR19, &IT83XX_INTC_IPOLR19, 0x08,
		IT83XX_IRQ_EXT_TIMER3},
	{&IT83XX_INTC_IELMR19, &IT83XX_INTC_IPOLR19, 0x10,
		IT83XX_IRQ_EXT_TIMER4},
	{&IT83XX_INTC_IELMR19, &IT83XX_INTC_IPOLR19, 0x20,
		IT83XX_IRQ_EXT_TIMER5},
	{&IT83XX_INTC_IELMR19, &IT83XX_INTC_IPOLR19, 0x40,
		IT83XX_IRQ_EXT_TIMER6},
	{&IT83XX_INTC_IELMR19, &IT83XX_INTC_IPOLR19, 0x80,
		IT83XX_IRQ_EXT_TIMER7},
	{&IT83XX_INTC_IELMR10, &IT83XX_INTC_IPOLR10, 0x01,
		IT83XX_IRQ_EXT_TMR8},
};
BUILD_ASSERT(ARRAY_SIZE(et_ctrl_regs) == EXT_TIMER_COUNT);

static void free_run_timer_reload_count(uint32_t us)
{
	/* microseconds to timer count, timer 3 and 4 combine mode */
	IT83XX_ETWD_ETXCNTLR(FREE_EXT_TIMER_H) =
		us >> (24 - TIMER_COUNT_1US_SHIFT);
	IT83XX_ETWD_ETXCNTLR(FREE_EXT_TIMER_L) =
		us << TIMER_COUNT_1US_SHIFT;
	/* bit1, timer re-start */
	IT83XX_ETWD_ETXCTRL(FREE_EXT_TIMER_L) |= (1 << 1);
}

static void free_run_timer_clear_pending_isr(void)
{
	/* w/c interrupt status */
	task_clear_pending_irq(et_ctrl_regs[FREE_EXT_TIMER_L].irq);
	task_clear_pending_irq(et_ctrl_regs[FREE_EXT_TIMER_H].irq);
}

static void free_run_timer_overflow(void)
{
	/* reload timer count */
	free_run_timer_reload_count(0xffffffff);
	/* w/c interrupt status */
	free_run_timer_clear_pending_isr();
	/* timer overflow */
	process_timers(1);
	update_exc_start_time();
}

static void event_timer_clear_pending_isr(void)
{
	/* w/c interrupt status */
	task_clear_pending_irq(et_ctrl_regs[EVENT_EXT_TIMER].irq);
}

uint32_t __hw_clock_source_read(void)
{
	uint32_t l_us, h_us;

	/* get timer count, timer 3 and 4 combine mode */
	h_us = IT83XX_ETWD_ETXCNTOR(FREE_EXT_TIMER_H);
	l_us = IT83XX_ETWD_ETXCNTOR(FREE_EXT_TIMER_L);
	/* timer 3 overflow, update timer count again */
	if (h_us != IT83XX_ETWD_ETXCNTOR(FREE_EXT_TIMER_H)) {
		h_us = IT83XX_ETWD_ETXCNTOR(FREE_EXT_TIMER_H);
		l_us = IT83XX_ETWD_ETXCNTOR(FREE_EXT_TIMER_L);
	}

	/* counting down timer, timer count to microseconds */
	return 0xffffffff - ((l_us >> TIMER_COUNT_1US_SHIFT) |
				(~h_us << (24 - TIMER_COUNT_1US_SHIFT)));
}

void __hw_clock_source_set(uint32_t ts)
{
	uint32_t start_us;

	/* counting down timer */
	start_us = 0xffffffff - ts;

	/* timer 3 and timer 4 are not enabled */
	if ((IT83XX_ETWD_ETXCTRL(FREE_EXT_TIMER_L) & 0x09) != 0x09) {
		/* bit3, timer 3 and timer 4 combine mode */
		IT83XX_ETWD_ETXCTRL(FREE_EXT_TIMER_L) |= (1 << 3);
		/* microseconds to timer count, clock source is 8mhz */
		ext_timer_ms(FREE_EXT_TIMER_H, EXT_PSR_8M_HZ, 0, 1,
			(start_us >> (24 - TIMER_COUNT_1US_SHIFT)), 1, 1);
		ext_timer_ms(FREE_EXT_TIMER_L, EXT_PSR_8M_HZ, 1, 1,
			(start_us << TIMER_COUNT_1US_SHIFT), 1, 1);
	} else {
		free_run_timer_clear_pending_isr();
		/* reload timer count only */
		free_run_timer_reload_count(start_us);
		task_enable_irq(et_ctrl_regs[FREE_EXT_TIMER_H].irq);
		task_enable_irq(et_ctrl_regs[FREE_EXT_TIMER_L].irq);
	}
}

void __hw_clock_event_set(uint32_t deadline)
{
	/* bit0, disable event timer */
	IT83XX_ETWD_ETXCTRL(EVENT_EXT_TIMER) &= ~(1 << 0);
	/* w/c interrupt status */
	event_timer_clear_pending_isr();
	/* microseconds to timer count */
	IT83XX_ETWD_ETXCNTLR(EVENT_EXT_TIMER) =
		(deadline - __hw_clock_source_read()) << TIMER_COUNT_1US_SHIFT;
	/* enable and re-start timer */
	IT83XX_ETWD_ETXCTRL(EVENT_EXT_TIMER) |= 0x03;
	task_enable_irq(et_ctrl_regs[EVENT_EXT_TIMER].irq);
}

uint32_t __hw_clock_event_get(void)
{
	uint32_t next_event_us = __hw_clock_source_read();

	/* bit0, event timer is enabled */
	if (IT83XX_ETWD_ETXCTRL(EVENT_EXT_TIMER) & (1 << 0)) {
		/* timer count to microseconds */
		next_event_us += (IT83XX_ETWD_ETXCNTOR(EVENT_EXT_TIMER) >>
			TIMER_COUNT_1US_SHIFT);
	}
	return next_event_us;
}

void __hw_clock_event_clear(void)
{
	/* stop event timer */
	ext_timer_stop(EVENT_EXT_TIMER, 1);
	event_timer_clear_pending_isr();
}

int __hw_clock_source_init(uint32_t start_t)
{
	/* enable free running timer */
	__hw_clock_source_set(start_t);
	/* init event timer */
	ext_timer_ms(EVENT_EXT_TIMER, EXT_PSR_8M_HZ, 0, 0, 0xffffffff, 1, 1);
	/* returns the IRQ number of event timer */
	return et_ctrl_regs[EVENT_EXT_TIMER].irq;
}

static void __hw_clock_source_irq(void)
{
	/* Determine interrupt number. */
	int irq = IT83XX_INTC_IVCT3 - 16;

	/* SW/HW interrupt of event timer. */
	if ((get_sw_int() == et_ctrl_regs[EVENT_EXT_TIMER].irq) ||
		(irq == et_ctrl_regs[EVENT_EXT_TIMER].irq)) {
		IT83XX_ETWD_ETXCNTLR(EVENT_EXT_TIMER) = 0xffffffff;
		IT83XX_ETWD_ETXCTRL(EVENT_EXT_TIMER) |= (1 << 1);
		event_timer_clear_pending_isr();
		process_timers(0);
		return;
	}

#ifdef CONFIG_WATCHDOG
	/*
	 * Both the external timer for the watchdog warning and the HW timer
	 * go through this irq. So, if this interrupt was caused by watchdog
	 * warning timer, then call that function.
	 */
	if (irq == et_ctrl_regs[WDT_EXT_TIMER].irq) {
		watchdog_warning_irq();
		return;
	}
#endif

#ifdef CONFIG_FANS
	if (irq == et_ctrl_regs[FAN_CTRL_EXT_TIMER].irq) {
		fan_ext_timer_interrupt();
		return;
	}
#endif

	if (irq == et_ctrl_regs[FREE_EXT_TIMER_L].irq) {
		/* w/c interrupt status */
		task_clear_pending_irq(et_ctrl_regs[FREE_EXT_TIMER_L].irq);
		/* disable timer 3 interrupt */
		task_disable_irq(et_ctrl_regs[FREE_EXT_TIMER_L].irq);
		/* reload timer count */
		if (IT83XX_ETWD_ETXCNTLR(FREE_EXT_TIMER_H)) {
			IT83XX_ETWD_ETXCNTLR(FREE_EXT_TIMER_L) =
				0xffffffff << TIMER_COUNT_1US_SHIFT;
			IT83XX_ETWD_ETXCNTLR(FREE_EXT_TIMER_H) -= 1;
			IT83XX_ETWD_ETXCTRL(FREE_EXT_TIMER_L) |= (1 << 1);
			update_exc_start_time();
		} else {
			free_run_timer_overflow();
		}
		return;
	}

	if (irq == et_ctrl_regs[FREE_EXT_TIMER_H].irq) {
		free_run_timer_overflow();
		return;
	}
}
DECLARE_IRQ(CPU_INT_GROUP_3, __hw_clock_source_irq, 1);

void ext_timer_start(enum ext_timer_sel ext_timer, int en_irq)
{
	/* enable external timer n */
	IT83XX_ETWD_ETXCTRL(ext_timer) |= 0x03;

	if (en_irq) {
		task_clear_pending_irq(et_ctrl_regs[ext_timer].irq);
		task_enable_irq(et_ctrl_regs[ext_timer].irq);
	}
}

void ext_timer_stop(enum ext_timer_sel ext_timer, int dis_irq)
{
	/* disable external timer n */
	IT83XX_ETWD_ETXCTRL(ext_timer) &= ~0x01;

	if (dis_irq)
		task_disable_irq(et_ctrl_regs[ext_timer].irq);
}

static void ext_timer_ctrl(enum ext_timer_sel ext_timer,
		enum ext_timer_clock_source ext_timer_clock,
		int start,
		int with_int,
		int32_t count)
{
	uint8_t intc_mask;

	/* rising-edge-triggered */
	intc_mask = et_ctrl_regs[ext_timer].mask;
	*et_ctrl_regs[ext_timer].mode |= intc_mask;
	*et_ctrl_regs[ext_timer].polarity &= ~intc_mask;

	/* clear interrupt status */
	task_clear_pending_irq(et_ctrl_regs[ext_timer].irq);

	/* These bits control the clock input source to the exttimer 3 - 8 */
	IT83XX_ETWD_ETXPSR(ext_timer) = ext_timer_clock;

	/* The count number of external timer n. */
	IT83XX_ETWD_ETXCNTLR(ext_timer) = count;

	ext_timer_stop(ext_timer, 0);
	if (start)
		ext_timer_start(ext_timer, 0);

	if (with_int)
		task_enable_irq(et_ctrl_regs[ext_timer].irq);
	else
		task_disable_irq(et_ctrl_regs[ext_timer].irq);
}

int ext_timer_ms(enum ext_timer_sel ext_timer,
		enum ext_timer_clock_source ext_timer_clock,
		int start,
		int with_int,
		int32_t ms,
		int first_time_enable,
		int raw)
{
	uint32_t count;

	if (raw) {
		count = ms;
	} else {
		if (ext_timer_clock == EXT_PSR_32P768K_HZ)
			count = MS_TO_COUNT(32768, ms);
		else if (ext_timer_clock == EXT_PSR_1P024K_HZ)
			count = MS_TO_COUNT(1024, ms);
		else if (ext_timer_clock == EXT_PSR_32_HZ)
			count = MS_TO_COUNT(32, ms);
		else if (ext_timer_clock == EXT_PSR_8M_HZ)
			count = 8000 * ms;
		else
			return -1;
	}

	if (count == 0)
		return -3;

	if (first_time_enable) {
		ext_timer_start(ext_timer, 0);
		ext_timer_stop(ext_timer, 0);
	}

	ext_timer_ctrl(ext_timer, ext_timer_clock, start, with_int, count);

	return 0;
}
