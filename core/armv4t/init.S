/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ARMv4 CPU initialization
 */

#include "config.h"

.section .text.vecttable

.macro vector name
.long \name\()_handler
.weak \name\()_handler
.set \name\()_handler, default_handler
.endm

.macro vector_irq number
.if \number < CONFIG_IRQ_COUNT
vector irq_\()\number
.endif
.endm

/* Exceptions vector */
vectors:
.long stack_end       		  @ initial stack pointer
.long reset           		  @ reset handler
vector nmi            		  @ NMI handler
vector hard_fault     		  @ HardFault handler
vector mpu_fault      		  @ MPU fault handler
vector bus_fault      		  @ Bus fault handler
vector usage_fault    		  @ Usage fault handler
.long 0               		  @ reserved
.long 0               		  @ reserved
.long 0               		  @ reserved
.long 0               		  @ reserved
vector svc            		  @ SWI
vector debug          		  @ Debug handler
.long 0               		  @ reserved
vector pendsv         		  @ PendSV handler
vector sys_tick       		  @ SysTick handler
vector_irq 0                      @ IRQ   0 handler

.text
.syntax unified
.code 32

.global reset
reset:
	/*
	 * Ensure we're in privileged mode with main stack.  Necessary if
	 * we've jumped directly here from another image after task_start().
	 */
	mov r0, #0           @ priv. mode / main stack / no floating point
	msr cpsr_c, r0
	/* isb                  @ ensure the write is done */

	/* Set the vector table on our current code */
	ldr r1, =vectors
	ldr r2, =0xE000ED08   /* VTABLE register in SCB*/
	str r1, [r2]

	/* Clear BSS */
	mov r0, #0
	ldr r1,_bss_start
	ldr r2,_bss_end
bss_loop:
	cmp r1, r2
	it lt
	strlt r0, [r1], #4
	blt bss_loop

#ifndef COMPILE_FOR_RAM
	/* Copy initialized data to Internal RAM */
	ldr r0,_ro_end
	ldr r1,_data_start
	ldr r2,_data_end
data_loop:
	ldr r3, [r0], #4
	cmp r1, r2
	it lt
	strlt r3, [r1], #4
	blt data_loop
#endif

	/*
	 * Set stack pointer.  Already done by Cortex-M hardware, but re-doing
	 * this here allows software to jump directly to the reset vector.
	 */
	ldr r0, =stack_end
	mov sp, r0

	/* Jump to C code */
	bl main

	/* That should not return.  If it does, loop forever. */
fini_loop:
	b fini_loop

/* Default exception handler */
.thumb_func
default_handler:
	b . /* exception_panic */

_bss_start:
.long __bss_start
_bss_end:
.long __bss_end
_data_start:
.long __data_start
_data_end:
.long __data_end
_ro_end:
.long __ro_end

/* Dummy functions to avoid linker complaints */
.global __aeabi_unwind_cpp_pr0
.global __aeabi_unwind_cpp_pr1
.global __aeabi_unwind_cpp_pr2
__aeabi_unwind_cpp_pr0:
__aeabi_unwind_cpp_pr1:
__aeabi_unwind_cpp_pr2:
	bx lr

/* Reserve space for system stack */
.section .bss.system_stack
stack_start:
.space CONFIG_STACK_SIZE, 0
stack_end:
.global stack_end

