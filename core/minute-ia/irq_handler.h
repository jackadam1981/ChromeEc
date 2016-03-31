/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helper to declare IRQ handling routines */

#ifndef __CROS_EC_IRQ_HANDLER_H
#define __CROS_EC_IRQ_HANDLER_H

#include "registers.h"

/* Helper macros to build the IRQ handler and priority struct names */
#define IRQ_HANDLER(irqname) CONCAT3(_irq_, irqname, _handler)
#define IRQ_PRIORITY(irqname) CONCAT2(prio_, irqname)
/*
 * Macro to connect the interrupt handler "routine" to the irq number "irq" and
 * ensure it is enabled in the interrupt controller with the right priority.
 *
 * Note: No 'naked' function support for x86, so function is implemented within
 * __asm__
 */
#define DECLARE_IRQ(irq, routine, vector) DECLARE_IRQ_(irq, routine, vector)
#define DECLARE_IRQ_(irq, routine, vector)			\
	void IRQ_HANDLER(irq)(void);				\
	void __keep routine(void);				\
	__asm__ (						\
		"_irq_"#irq"_handler:\n"			\
			"pusha\n"				\
			"mov %esp, %ebp\n"			\
			"movl __isr_stack_ptr, %esp\n"		\
			"push %ebp\n"				\
			"movl %esp, %ebp\n"			\
			"add  $1, __in_isr\n"			\
			"call "#routine"\n"			\
			"sub  $1, __in_isr\n"			\
			"movl $"#vector", (0xFEC00040)\n"	\
			"push $0\n"				\
			"push $0\n"				\
			"call switch_handler\n"			\
			"addl $0x08, %esp\n"			\
			"test %eax, %eax\n"			\
			"pop %esp\n"				\
			"je 1f\n"				\
			"movl current_task, %eax\n"		\
			"movl %esp, (%eax)\n"			\
			"movl next_task, %eax\n"		\
			"movl %eax, current_task\n"		\
			"movl (%eax), %esp\n"			\
			"1:\n"					\
			"movl $0x00, (0xFEE000B0)\n" 		\
			"popa\n"				\
			"iret\n"				\
		);

#endif  /* __CROS_EC_IRQ_HANDLER_H */
