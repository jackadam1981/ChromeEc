/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* tiny substitute of the runtime layer */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "cpu.h"
#include "debug.h"
#include "registers.h"
#include "timer.h"
#include "util.h"

volatile uint32_t last_event;

timestamp_t get_time(void)
{
	timestamp_t t;

	t.le.lo = STM32_TIM32_CNT(2);
	t.le.hi = 0;
	return t;
}

void udelay(unsigned us)
{
	unsigned t0 = STM32_TIM32_CNT(2);
	while ((STM32_TIM32_CNT(2) - t0) < us)
		;
}

uint32_t task_wait_event(int timeout_us)
{
	uint32_t evt;

	if (0) {
	while (1) {
		debug_printf("GPIO_A %08x A7 %d CPU_NVIC_PEND %08x ADC1 %d \n",
			STM32_GPIO_IDR(GPIO_A), !(STM32_GPIO_IDR(GPIO_A) & 0x80),
			CPU_NVIC_UNPEND(0), adc_read_channel(1));
	}
	}

	/* TODO set timeout on timer */
	if (timeout_us > 0) {
		STM32_TIM32_CCR1(2) = STM32_TIM32_CNT(2) + timeout_us;
		/* TODO STM32_TIM_SR(2) = 0; */ /* clear match flag */
		/* TODO STM32_TIM_DIER(2) = 2; */ /*  match interrupt */
	}

	/* sleep until next interrupt */
	asm volatile("wfi");

	/* TODO STM32_TIM_DIER(2) = 0; */ /* disable match interrupt */
	evt = last_event;
	last_event = 0;

	return evt;
}

void task_enable_irq(int irq)
{
	CPU_NVIC_EN(0) = 1 << irq;
}

void task_disable_irq(int irq)
{
	CPU_NVIC_DIS(0) = 1 << irq;
}

void task_clear_pending_irq(int irq)
{
	CPU_NVIC_UNPEND(0) = 1 << irq;
}

/* --- stubs --- */
void __hw_timer_enable_clock(int n, int enable)
{ /* Done in hardware init */ }

void usleep(unsigned us)
{ /* Used only as a workaround */ }
