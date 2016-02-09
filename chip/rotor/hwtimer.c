/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hardware timer driver for Rotor MCU */

#include "common.h"
#include "hwtimer.h"
#include "registers.h"
#include "task.h"

/*
 * Timer 0 is the clock timer.
 * Timer 1 is the event timer.
 */

void __hw_clock_event_set(uint32_t deadline)
{
	__hw_clock_event_clear();

	/* Set the timer load count to the deadline. */
	ROTOR_MCU_TMR_TNLC(1) = 0xFFFFFFFF - (ROTOR_MCU_TMR_TNCV(0) - deadline);

	/* Enable the timer. */
	ROTOR_MCU_TMR_TNCR(1) |= (1 << 0);
}

uint32_t __hw_clock_event_get(void)
{
	/* Get the time of the next programmed deadline. */
	return (0xFFFFFFFF - ROTOR_MCU_TMR_TNCV(0)) + ROTOR_MCU_TMR_TNCV(1);
}

void __hw_clock_event_clear(void)
{
	/* Disable the event timer.  This also clears any pending interrupt. */
	ROTOR_MCU_TMR_TNCR(1) &= ~(1 << 0);
}

/* Triggered when Timer 1 reaches 0. */
void __hw_clock_event_irq(void)
{
	/*
	 * Clear the event which disables the timer and clears the pending
	 * interrupt.
	 */
	__hw_clock_event_clear();

	/* Process timers now. */
	process_timers(0);
}
DECLARE_IRQ(ROTOR_MCU_IRQ_TIMER_1, __hw_clock_event_irq, 1);

uint32_t __hw_clock_source_read(void)
{
	return 0xFFFFFFFF - ROTOR_MCU_TMR_TNCV(0);
}

void __hw_clock_source_set(uint32_t ts)
{
	ROTOR_MCU_TMR_TNCV(0) = 0xFFFFFFFF - ts;
}

/* Triggered when Timer 0 reaches 0. */
void __hw_clock_source_irq(void)
{
	/*
	 * Clear the interrupt by reading the TNEOI register.  Reading from this
	 * register returns all zeroes.
	 */
	if (ROTOR_MCU_TMR_TNEOI(0))
		;

	/* Process timers indicating the overflow event. */
	process_timers(1);
}
DECLARE_IRQ(ROTOR_MCU_IRQ_TIMER_0, __hw_clock_source_irq, 1);

void __hw_timer_enable_clock(int n, int enable)
{
	/* Should be already be configured. */
}

int __hw_clock_source_init(uint32_t start_t)
{
	/*
	 * Use Timer 0 as clock.  The timerbase should be configured for 1us
	 * period.
	 *
	 * There's also no match functionality, so set up a second timer, Timer
	 * 1, to handle events.
	 */

	/* Disable the timers. */
	ROTOR_MCU_TMR_TNCR(0) &= ~(1 << 0);
	ROTOR_MCU_TMR_TNCR(1) &= ~(1 << 0);

	/*
	 * Timer 0
	 *
	 * Unmask interrupt, set free running mode, and disable PWM.
	 */
	ROTOR_MCU_TMR_TNCR(0) &= ~(7 << 1);
	/* Enable Timer 0. */
	ROTOR_MCU_TMR_TNCR(0) |= (1 << 0);

	/* Use the specified start timer value. */
	__hw_clock_source_set(start_t);

	/*
	 * Timer 1
	 *
	 * Unmask interrupt, set user-defined count mode, and disable PWM.
	 */
	ROTOR_MCU_TMR_TNCR(1) = (1 << 1);

	/* Enable interrupts. */
	task_enable_irq(ROTOR_MCU_IRQ_TIMER_0);
	task_enable_irq(ROTOR_MCU_IRQ_TIMER_1);

	/* Return event timer IRQ number. */
	return ROTOR_MCU_IRQ_TIMER_1;
}

void hwtimer_setup_watchdog(void)
{
}

void hwtimer_reset_watchdog(void)
{
}
