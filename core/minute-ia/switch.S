/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Context swtching for x86
 */

#include "config.h"

.text

.extern __isr_stack_ptr
.extern __in_isr
.extern current_task
.extern next_task

.global __task_start
.global __switchto
.global default_int_handler

/* Start the task scheduling.  current_task (t0) is the task to start */
.align 4
.func __task_start
__task_start:
	movl current_task, %eax
	movl (%eax), %esp
	popa
	iret
.endfunc

/* Default interrupt handler - just prints error and does EOI */
.align 4
.func default_int_handler
default_int_handler:

	pusha
	movl __in_isr, %eax
	test %eax, %eax
	jne  1f
	movl %esp, %ebp
	movl __isr_stack_ptr, %esp
	push %ebp
	movl %esp, %ebp
	1:
	add $1, __in_isr
	call unhandled_vector
	sub $1, __in_isr
	pop %esp
	#movl %eax, (0xFEC00040)
	movl $0x00, (0xFEE000B0)

	popa
	iret

.endfunc

/* Switches from one task to another if ready */
.align 4
.func __switchto
__switchto:

	addl $1, __in_isr

	/* Save current task */
	pusha

	/* Switch stacks. */
	movl    %esp, %ebp
	movl    __isr_stack_ptr, %esp

	/* Push the task's stack pointer onto the system stack. */
	push %ebp
	movl %esp, %ebp

	subl $0x8, %esp
	push %ecx
	push %edx
	call switch_handler
	addl $0x10, %esp

	test %eax, %eax
	pop %esp
	je 2f

	/* Switch to new task */
	movl current_task, %eax
	movl %esp, (%eax)
	movl next_task, %eax
	movl %eax, current_task
	movl (%eax), %esp

	2:
	subl $1, __in_isr
	/* Restore general purpose registers. */
	popa
	iret

.endfunc
