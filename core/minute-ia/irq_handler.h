/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helper to declare IRQ handling routines */

#ifndef __CROS_EC_IRQ_HANDLER_H
#define __CROS_EC_IRQ_HANDLER_H

#include "registers.h"
#include "task_defs.h"

#ifdef CONFIG_FPU
#define save_fpu_ctx	"movl "USE_FPU_OFFSET_STR"(%eax), %ebx\n"	\
			"test %ebx, %ebx\n"				\
			"jz 9f\n"					\
			"fnsave "FPU_CTX_OFFSET_STR"(%eax)\n"		\
			"9:\n"

#define rstr_fpu_ctx	"movl "USE_FPU_OFFSET_STR"(%eax), %ebx\n"	\
			"test %ebx, %ebx\n"				\
			"jz 9f\n"					\
			"frstor "FPU_CTX_OFFSET_STR"(%eax)\n"		\
			"9:\n"
#else
#define save_fpu_ctx
#define rstr_fpu_ctx
#endif

#ifdef CONFIG_TASK_PROFILING
#define task_start_irq_handler_call(vector) \
			"push $"#vector"\n"	\
			"call task_start_irq_handler\n"	\
			"addl $0x4, %esp\n"
#else
#define task_start_irq_handler_call(vector)
#endif


struct irq_data {
	void (*routine)(void);
	int irq;
};

/* Helper macros to build the IRQ handler and priority struct names */
#define IRQ_HANDLER(irqname) CONCAT3(_irq_, irqname, _handler)
#define IRQ_PRIORITY(irqname) CONCAT2(prio_, irqname)
/*
 * Macro to connect the interrupt handler "routine" to the irq number "irq" and
 * ensure it is enabled in the interrupt controller with the right priority.
 *
 * Note: No 'naked' function support for x86, so function is implemented within
 * __asm__
 * Note: currently we don't allow nested irq handling
 */
/* Each irq has a irq_data structure placed in .rodata.irqs section,
 * to be used for dynamically setting up interrupt gates */
#define DECLARE_IRQ(irq, routine)					\
	void __keep routine(void);					\
	__asm__ (".section .rodata.irqs\n");				\
	const struct irq_data __keep CONCAT4(__irq_, irq, _, routine)	\
	__attribute__((section(".rodata.irqs")))= { routine, irq};
#endif  /* __CROS_EC_IRQ_HANDLER_H */
