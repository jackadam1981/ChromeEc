/* Copyright (c) The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is a low priority interrupt(that will only happen after all other irqs)
 * that is responsible for calling the scheduler.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "registers.h"
#include "task.h"

#ifndef CHIP_FAMILY_STM32F0
#error This is only supported on STM32F0
#endif

#if defined(CONFIG_STREAM_USART3) || defined(CONFIG_STREAM_USART4)
#error USART3/4 is needed for scheduling
#endif

#define POSTINTERRUPT_IRQ STM32_IRQ_USART3_4

void scheduler_postinterrupt_enable(void) {
	task_enable_irq(POSTINTERRUPT_IRQ);
}

/*TODO: inline this in asm */
void scheduler_postinterrupt_trigger(void) {
	task_trigger_irq(POSTINTERRUPT_IRQ);
}

/*TODO: inline this in asm */
void _scheduler_postinterrupt_clear(void) {
	task_clear_pending_irq(POSTINTERRUPT_IRQ);
}

void IRQ_HANDLER(POSTINTERRUPT_IRQ)(void) __attribute__((naked));
void IRQ_HANDLER(POSTINTERRUPT_IRQ)(void)
{
	asm volatile(
		"mov r0, lr\n"
		/* Must push registers in pairs to keep 64-bit aligned
		* stack for ARM EABI. */
		"push {r0, lr}\n"
		"bl _scheduler_postinterrupt_clear\n"
		/* ensure we have priority 0 during re-scheduling */
		"mov r0, #0\n"
		"mov r1, #0\n"
		"cpsid i\n isb\n"
		/* re-schedule the highest priority task */
		"bl svc_handler\n"
		/* enable interrupts and return from exception */
		"cpsie i\n"
		"pop {r0,pc}\n"
	);
}

const struct irq_priority IRQ_PRIORITY(POSTINTERRUPT_IRQ)
	__attribute__((section(".rodata.irqprio")))
		= {POSTINTERRUPT_IRQ, 3}; /* put the postinterrupt at the lowest
					     priority */
/* TODO: Check that this is indeed the lowest we can get. */
