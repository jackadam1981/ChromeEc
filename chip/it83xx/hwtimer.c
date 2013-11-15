/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hardware timers driver */

#include "common.h"
#include "hooks.h"
#include "hwtimer.h"
#include "panic.h"
#include "registers.h"
#include "task.h"
#include "timer.h"

static volatile uint32_t time_us;

static int event_set;
static uint32_t next_event_time;

void __hw_clock_event_set(uint32_t deadline)
{
	next_event_time = deadline;
	event_set = 1;
}

uint32_t __hw_clock_event_get(void)
{
	return next_event_time;
}

void __hw_clock_event_clear(void)
{
	event_set = 0;
}

uint32_t __hw_clock_source_read(void)
{
	return time_us;
}

void __hw_clock_source_set(uint32_t ts)
{
	time_us = ts;
}


static void __hw_clock_source_irq(void)
{
	panic_puts("HWTMR\n");
	/* clear interrupt status */
	IT83XX_INTC_ISR7 = 0x40;

	time_us++;

	/*
	 * Find expired timers and set the new timer deadline; check the IRQ
	 * status to determine if the free-running counter overflowed.
	 */
	if(event_set && (time_us == next_event_time))
		process_timers(0);
	else if (time_us == 0)
		process_timers(1);
}
DECLARE_IRQ(1 /*should 3 the CPU_INT number for IT83XX_IRQ_TMR_B0 */, __hw_clock_source_irq, 1);

static void setup_gpio(void)
{
	/* TMB0 enabled */
	IT83XX_GPIO_GRC2 |= 0x04;

	/* Pin muxing */
	IT83XX_GPIO_GPCRF0 = 0x00;	/* TMB0 */
}

static void hw_timer_enable_int(void)
{
	/* clear interrupt status */
	IT83XX_INTC_ISR7 = 0x40;

	/* enable interrupt B0 */
	IT83XX_INTC_IER7 = 0x40;
}

int __hw_clock_source_init(uint32_t start_t)
{
	time_us = start_t;

	/* GPIO module should do this. */
	setup_gpio();

#if PLL_CLOCK == 48000000
	/* Set prescaler divider value (/8 for B and /1 for A). */
	IT83XX_TMR_PRSC = 0xFF;

	/*
	 * Tim A: 16 bit pulse mode, 8MHz clock
	 * Tim B: 8  bit pulse mode, 8MHz clock.
	 */
	/* TODO: depends on clock source */
	IT83XX_TMR_GCSMS = 0x15;
#else
#error "Support only for PLL clock speed of 48MHz."
#endif

	/* Set the 16-bit cycle time, duty time for timers. */
	IT83XX_TMR_CTR_A0 = 0xff;
	IT83XX_TMR_CTR_A1 = 0xff;
	IT83XX_TMR_CTR_B0 = 0xFF;	// cycle set for 1us
	IT83XX_TMR_DCR_B0 = 0x04;

	/* Enable the cycle time interrupt for timer B0. */
	IT83XX_TMR_TMRIE |= 0x10;

	hw_timer_enable_int();

	/* Enable TMR clock counter. */
	IT83XX_TMR_TMRCE |= 0x02;

	return 0;
}
