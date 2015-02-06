/* Copyright (c) The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is a low priority interrupt(that will only happen after all other irqs)
 * that is responsible for calling the scheduler.
 */

#include "common.h"
#include "cpu.h"
#include "console.h"
#include "gpio.h"
#include "registers.h"
#include "task.h"

void scheduler_postinterrupt_enable(void)
{
	/* Set lowest priority for PendSV */
	CPU_NVIC_SHCSR3 |= (0xff << 16);
}

/*TODO: inline this in asm */
void scheduler_postinterrupt_trigger(void)
{
	CPU_SBC_ICSR |= (1 << 28);
}

/*TODO: inline this in asm */
void _scheduler_postinterrupt_clear(void)
{
	CPU_SBC_ICSR |= (1 << 27);
}

void pendsv_handler(void) __attribute__((naked));
void pendsv_handler(void)
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
