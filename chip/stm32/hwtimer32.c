/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hardware 32-bit timer driver */

#include "clock.h"
#include "common.h"
#include "hooks.h"
#include "hwtimer.h"
#include "panic.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "watchdog.h"

/*
 * Trigger select mapping for slave timer from master timer.  This is
 * unfortunately not very straightforward; there's no tidy way to do this
 * algorithmically.  To avoid burning memory for a lookup table, use macros to
 * compute the offset.  This also has the benefit that compilation will fail if
 * an unsupported master/slave pairing is used.
 */
#ifdef CHIP_FAMILY_STM32F0
/*
 * Slave        Master
 *     1    15  2  3 17
 *     2     1 15  3 14
 *     3     1  2 15 14
 *    15     2  3 16 17
 *     --------------------
 *     ts =  0  1  2  3
 */
#define STM32_TIM_TS_SLAVE_1_MASTER_15  0
#define STM32_TIM_TS_SLAVE_1_MASTER_2   1
#define STM32_TIM_TS_SLAVE_1_MASTER_3   2
#define STM32_TIM_TS_SLAVE_1_MASTER_17  3
#define STM32_TIM_TS_SLAVE_2_MASTER_1   0
#define STM32_TIM_TS_SLAVE_2_MASTER_15  1
#define STM32_TIM_TS_SLAVE_2_MASTER_3   2
#define STM32_TIM_TS_SLAVE_2_MASTER_14  3
#define STM32_TIM_TS_SLAVE_3_MASTER_1   0
#define STM32_TIM_TS_SLAVE_3_MASTER_2   1
#define STM32_TIM_TS_SLAVE_3_MASTER_15  2
#define STM32_TIM_TS_SLAVE_3_MASTER_14  3
#define STM32_TIM_TS_SLAVE_15_MASTER_2  0
#define STM32_TIM_TS_SLAVE_15_MASTER_3  1
#define STM32_TIM_TS_SLAVE_15_MASTER_16 2
#define STM32_TIM_TS_SLAVE_15_MASTER_17 3
#else /* !CHIP_FAMILY_STM32F0 */
/*
 * Slave        Master
 *     1    15  2  3  4  (STM32F100 only)
 *     2     9 10  3  4
 *     3     9  2 11  4
 *     4    10  2  3  9
 *     9     2  3 10 11  (STM32L15x only)
 *     --------------------
 *     ts =  0  1  2  3
 */
#define STM32_TIM_TS_SLAVE_1_MASTER_15 0
#define STM32_TIM_TS_SLAVE_1_MASTER_2  1
#define STM32_TIM_TS_SLAVE_1_MASTER_3  2
#define STM32_TIM_TS_SLAVE_1_MASTER_4  3
#define STM32_TIM_TS_SLAVE_2_MASTER_9  0
#define STM32_TIM_TS_SLAVE_2_MASTER_10 1
#define STM32_TIM_TS_SLAVE_2_MASTER_3  2
#define STM32_TIM_TS_SLAVE_2_MASTER_4  3
#define STM32_TIM_TS_SLAVE_3_MASTER_9  0
#define STM32_TIM_TS_SLAVE_3_MASTER_2  1
#define STM32_TIM_TS_SLAVE_3_MASTER_11 2
#define STM32_TIM_TS_SLAVE_3_MASTER_4  3
#define STM32_TIM_TS_SLAVE_4_MASTER_10 0
#define STM32_TIM_TS_SLAVE_4_MASTER_2  1
#define STM32_TIM_TS_SLAVE_4_MASTER_3  2
#define STM32_TIM_TS_SLAVE_4_MASTER_9  3
#define STM32_TIM_TS_SLAVE_9_MASTER_2  0
#define STM32_TIM_TS_SLAVE_9_MASTER_3  1
#define STM32_TIM_TS_SLAVE_9_MASTER_10 2
#define STM32_TIM_TS_SLAVE_9_MASTER_11 3
#endif /* !CHIP_FAMILY_STM32F0 */
#define TSMAP(slave, master) \
	CONCAT4(STM32_TIM_TS_SLAVE_, slave, _MASTER_, master)

/*
 * Timers are defined per board.  This gives us flexibility to work around
 * timers which are dedicated to board-specific PWM sources.
 */
#define IRQ_TIM(n) CONCAT2(STM32_IRQ_TIM, n)
#define IRQ_MSB IRQ_TIM(TIM_CLOCK_MSB)
#define IRQ_LSB IRQ_TIM(TIM_CLOCK32)
#define IRQ_WD  IRQ_TIM(TIM_WATCHDOG)

/* TIM1 has fancy names for its IRQs; remap count-up IRQ for the macro above */
#ifdef CHIP_FAMILY_STM32F0
#define STM32_IRQ_TIM1 STM32_IRQ_TIM1_BRK_UP_TRG
#else /* !CHIP_FAMILY_STM32F0 */
#define STM32_IRQ_TIM1 STM32_IRQ_TIM1_UP_TIM16
#endif /* !CHIP_FAMILY_STM32F0 */

#define TIM_BASE(n) CONCAT3(STM32_TIM, n, _BASE)
#define TIM_WD_BASE TIM_BASE(TIM_WATCHDOG)

void __hw_clock_event_set(uint32_t deadline)
{
	/* set the match on the deadline */
	STM32_TIM32_CCR1(TIM_CLOCK32) = deadline;
	/* Clear the match flags */
	STM32_TIM_SR(TIM_CLOCK32) = ~2;
	/* Set the match interrupt */
	STM32_TIM_DIER(TIM_CLOCK32) |= 2;
}

uint32_t __hw_clock_event_get(void)
{
	return STM32_TIM32_CCR1(TIM_CLOCK32);
}

void __hw_clock_event_clear(void)
{
	/* Disable the match interrupts */
	STM32_TIM_DIER(TIM_CLOCK32) &= ~2;
}

uint32_t __hw_clock_source_read(void)
{
	return STM32_TIM32_CNT(TIM_CLOCK32);
}

void __hw_clock_source_set(uint32_t ts)
{
	STM32_TIM32_CNT(TIM_CLOCK32) = ts;
}

void __hw_clock_source_irq(void)
{
	uint32_t stat_tim = STM32_TIM_SR(TIM_CLOCK32);

	/* Clear status */
	STM32_TIM_SR(TIM_CLOCK32) = 0;

	/*
	 * Find expired timers and set the new timer deadline
	 * signal overflow if the update interrupt flag is set.
	 */
	process_timers(stat_tim & 0x01);
}
DECLARE_IRQ(IRQ_TIM(TIM_CLOCK32), __hw_clock_source_irq, 1);

void __hw_timer_enable_clock(int n, int enable)
{
	volatile uint32_t *reg;
	uint32_t mask = 0;

	/*
	 * Mapping of timers to reg/mask is split into a few different ranges,
	 * some specific to individual chips.
	 */
#if defined(CHIP_FAMILY_STM32F) || defined(CHIP_FAMILY_STM32F0)
	if (n == 1) {
		reg = &STM32_RCC_APB2ENR;
		mask = STM32_RCC_PB2_TIM1;
	}
#elif defined(CHIP_FAMILY_STM32L)
	if (n >= 9 && n <= 11) {
		reg = &STM32_RCC_APB2ENR;
		mask = STM32_RCC_PB2_TIM9 << (n - 9);
	}
#endif

#if defined(CHIP_FAMILY_STM32F0)
	if (n >= 15 && n <= 17) {
		reg = &STM32_RCC_APB2ENR;
		mask = STM32_RCC_PB2_TIM15 << (n - 15);
	}
	if (n == 14) {
		reg = &STM32_RCC_APB1ENR;
		mask = STM32_RCC_PB1_TIM14;
	}
#endif
	if (n >= 2 && n <= 7) {
		reg = &STM32_RCC_APB1ENR;
		mask = STM32_RCC_PB1_TIM2 << (n - 2);
	}

	if (!mask)
		return;

	if (enable)
		*reg |= mask;
	else
		*reg &= ~mask;
}

static void update_prescaler(void)
{
	/*
	 * Pre-scaler value :
	 * the timer is incrementing every microsecond
	 *
	 * This will take effect at the next update event (when the current
	 * prescaler counter ticks down, or if forced via EGR).
	 */
	STM32_TIM_PSC(TIM_CLOCK32) = (clock_get_freq() / SECOND) - 1;
}
DECLARE_HOOK(HOOK_FREQ_CHANGE, update_prescaler, HOOK_PRIO_DEFAULT);

int __hw_clock_source_init(uint32_t start_t)
{
	/* Enable TIM peripheral block clocks */
	__hw_timer_enable_clock(TIM_CLOCK32, 1);

	/*
	 * Timer configuration : Upcounter, counter disabled, update event only
	 * on overflow.
	 */
	STM32_TIM_CR1(TIM_CLOCK32) = 0x0004;
	/* No special configuration */
	STM32_TIM_CR2(TIM_CLOCK32) = 0x0000;
	STM32_TIM_SMCR(TIM_CLOCK32) = 0x0000;

	/* Auto-reload value : 32-bit free-running counter */
	STM32_TIM32_ARR(TIM_CLOCK32) = 0xffffffff;

	/* Update prescaler */
	update_prescaler();

	/* Reload the pre-scaler */
	STM32_TIM_EGR(TIM_CLOCK32) = 0x0001;

	/* Set up the overflow interrupt */
	STM32_TIM_DIER(TIM_CLOCK32) = 0x0001;

	/* Start counting */
	STM32_TIM_CR1(TIM_CLOCK32) |= 1;

	/* Override the count with the start value now that counting has
	 * started. */
	__hw_clock_source_set(start_t);

	/* Enable timer interrupts */
	task_enable_irq(IRQ_TIM(TIM_CLOCK32));

	return IRQ_TIM(TIM_CLOCK32);
}

#ifdef CONFIG_WATCHDOG_HELP



#define TIM_BASE(n) CONCAT3(STM32_TIM, n, _BASE)
#define TIM_WD_BASE TIM_BASE(TIM_WATCHDOG)


#define IRQ_WD  IRQ_TIM(TIM_WATCHDOG)





void watchdog_check(uint32_t excep_lr, uint32_t excep_sp)
{
	struct timer_ctlr *timer = (struct timer_ctlr *)TIM_WD_BASE;

	/* clear status */
	timer->sr = 0;

	watchdog_trace(excep_lr, excep_sp);
}

void IRQ_HANDLER(IRQ_WD)(void) __attribute__((naked));
void IRQ_HANDLER(IRQ_WD)(void)
{
	/* Naked call so we can extract raw LR and SP */
	asm volatile("mov r0, lr\n"
		     "mov r1, sp\n"
		     /* Must push registers in pairs to keep 64-bit aligned
		      * stack for ARM EABI.  This also conveninently saves
		      * R0=LR so we can pass it to task_resched_if_needed. */
		     "push {r0, lr}\n"
		     "bl watchdog_check\n"
		     "pop {r0, lr}\n"
		     "b task_resched_if_needed\n");
}
const struct irq_priority IRQ_PRIORITY(IRQ_WD)
	__attribute__((section(".rodata.irqprio")))
		= {IRQ_WD, 0}; /* put the watchdog at the highest
					    priority */

void hwtimer_setup_watchdog(void)
{
	struct timer_ctlr *timer = (struct timer_ctlr *)TIM_WD_BASE;

	/* Enable clock */
	__hw_timer_enable_clock(TIM_WATCHDOG, 1);

	/*
	 * Timer configuration : Down counter, counter disabled, update
	 * event only on overflow.
	 */
	timer->cr1 = 0x0014 | (1 << 7);

	/* TIM (slave mode) uses TIM_CLOCK32 as internal trigger */
	timer->smcr = 0x0007 | (TSMAP(TIM_WATCHDOG, TIM_CLOCK32) << 4);

	/*
	 * The auto-reload value is based on the period between rollovers for
	 * TIM_CLOCK32. Since TIM_CLOCK32 runs at 1MHz, it will overflow
	 * in 65.536ms. We divide our required watchdog period by this amount
	 * to obtain the number of times TIM_CLOCK32 can overflow before we
	 * generate an interrupt.
	 */
	timer->arr = timer->cnt = CONFIG_AUX_TIMER_PERIOD_MS * MSEC / (1 << 16);

	/* count on every TIM_CLOCK32 overflow */
	timer->psc = 0;

	/* Reload the pre-scaler from arr when it goes below zero */
	timer->egr = 0x0000;

	/* setup the overflow interrupt */
	timer->dier = 0x0001;

	/* Start counting */
	timer->cr1 |= 1;

	/* Enable timer interrupts */
	task_enable_irq(IRQ_WD);
}

void hwtimer_reset_watchdog(void)
{
	struct timer_ctlr *timer = (struct timer_ctlr *)TIM_WD_BASE;

	timer->cnt = timer->arr;
}

#endif  /* defined(CONFIG_WATCHDOG) */
