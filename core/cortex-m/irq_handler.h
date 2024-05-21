/* Copyright 2014 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helper to declare IRQ handling routines */

#ifndef __CROS_EC_IRQ_HANDLER_H
#define __CROS_EC_IRQ_HANDLER_H

#ifdef CONFIG_TASK_PROFILING
#define TASK_START_IRQ_HANDLER , "i"(task_start_irq_handler)
#define TASK_START_IRQ_CALL(arg) \
	"mov r0, lr\n"           \
	"bl %" #arg "\n"
#else
#define TASK_START_IRQ_HANDLER
#define TASK_START_IRQ_CALL(arg)
#endif

/* Helper macros to build the IRQ handler and priority struct names */
#define IRQ_HANDLER(irqname) CONCAT3(irq_, irqname, _handler)
#define IRQ_PRIORITY(irqname) CONCAT2(prio_, irqname)
/*
 * Macro to connect the interrupt handler "routine" to the irq number "irq" and
 * ensure it is enabled in the interrupt controller with the right priority.
 */
/* clang-format off */
#define DECLARE_IRQ(irq, routine, priority) DECLARE_IRQ_(irq, routine, priority)
#define DECLARE_IRQ_(irq, routine, priority)                               \
	void IRQ_HANDLER(irq)(void);                                       \
	typedef struct {                                                   \
		int fake[irq >= CONFIG_IRQ_COUNT ? -1 : 1];                \
	} irq_num_check_##irq;                                             \
	static void __keep routine(void);                                  \
	__attribute__((naked)) void IRQ_HANDLER(irq)(void)                 \
	{                                                                  \
		asm(".thumb_func\n"                                        \
		    "	push {lr}\n"                                       \
		    TASK_START_IRQ_CALL(2)                                 \
		    "	bl %0\n" /* Call IRQ routine */                    \
		    "	pop {lr}\n"                                        \
		    "	mov r0, lr\n"                                      \
		    "	b %1\n" /* Reschedule task if needed */            \
		    :                                                      \
		    : "i"(routine),                                        \
		      "i"(task_resched_if_needed)                          \
		      TASK_START_IRQ_HANDLER);                             \
	}                                                                  \
	const struct irq_priority __keep IRQ_PRIORITY(irq) __attribute__(( \
		weak, section(".rodata.irqprio"))) = { irq, priority }
/* clang-format on */
#endif /* __CROS_EC_IRQ_HANDLER_H */
