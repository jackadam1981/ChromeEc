/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * x86 task swtching and interrupt handling
 */

#include "config.h"
#include "registers.h"
#include "task_defs.h"

.text

.extern current_task
.extern next_task

.global __task_start
.global __switchto
.global default_int_handler
.global sw_irq_handler

# Start the task scheduling. Start current_task (hook_task)
# This function is not an ISR but imitates the sequence.
.align 4
.func __task_start
__task_start:
	movl 0x4(%esp), %ecx
	movl current_task, %eax
	movl (%eax), %esp
#ifdef CONFIG_FPU
	movl USE_FPU_OFFSET(%eax), %ebx
	test %ebx, %ebx
	jz 1f
	frstor FPU_CTX_OFFSET(%eax)
	1:
#endif
	movl $0x1, (%ecx)       # first task is ready. set start_called = 1
	popa
	iret
.endfunc

# Default interrupt handler - to handle exceptions
# and prints error
.align 4
.func default_int_handler
default_int_handler:

	pusha
	ASM_LOCK_PREFIX addl $1, __in_isr

	movl %esp, %eax
	movl $stack_end, %esp		# use system stack
	push %eax			# push sp of preempted context

	call unhandled_vector		# Handle system interrupts and
					# unregistered user interrupts
	pop %esp			# restore sp of preempted context
	# unhandled_vector call loads eax with vector for comparison
	cmpl $LAPIC_SPURIOUS_INT_VECTOR, %eax
	je 1f				# No EOI for LAPIC_SPURIOUS_INT_VECTOR

	movl %eax, IOAPIC_EOI_REG	# Indicate completion of servicing the
					# interrupt to IOAPIC first
	movl $0x00, LAPIC_EOI_REG	# Indicate completion of servicing the
					# interrupt to LAPIC next
	1:
	# Ensure we balance the __in_isr counter
	ASM_LOCK_PREFIX subl $1, __in_isr
	popa
	iret

.endfunc

# argument irq is input and it has irq number
.macro irq_handler_sw irq
	push \irq
#ifdef CONFIG_TASK_PROFILING
	call task_start_irq_handler
#endif
	call call_irq_service_routine
	addl $0x04, %esp
.endm

.macro irq_handler_switch
#ifdef CONFIG_TASK_PROFILING
	push $0x00
	call task_start_irq_handler
	addl $0x04, %esp
#endif
.endm

.macro irq_handler_common desched resched handler handler_arg
	# __schedule() copies 'resched' to %ecx and 'desched' to %edx before
	movl %esp, %eax
	movl $stack_end, %esp		# use system stack
	push %eax			# push sp of preempted context

	# we push resched and desched here since it can be clobbered by handler
	push \resched
	push \desched

	\handler \handler_arg		# call handler(hw, sw)

	call switch_handler		# switch task if needed
	addl $0x08, %esp

	pop %esp			# restore sp of preempted context

	test %eax, %eax			# Check if task switch required
	jz 1f

	movl current_task, %eax

#ifdef CONFIG_FPU
	movl USE_FPU_OFFSET(%eax), %ecx
	test %ecx, %ecx
	jz 2f
	fnsave FPU_CTX_OFFSET(%eax) # Save current FPU context(current->fp_ctx)
	2:
#endif

	# Save SP of current task and switch to new task
	movl %esp, (%eax)
	movl next_task, %eax
	movl %eax, current_task
	movl (%eax), %esp

#ifdef CONFIG_FPU
	movl USE_FPU_OFFSET(%eax), %ecx
	test %ecx, %ecx
	jz 1f
	frstor FPU_CTX_OFFSET(%eax)		# Restore next FPU context
#endif

	1:
.endm

.align 4
.func sw_irq_handler
sw_irq_handler:
	pusha
	ASM_LOCK_PREFIX addl $1, __in_isr

	# call sw irq handler with irq number(%ecx) from task_trigger_irq
	irq_handler_common $0 $0 irq_handler_sw %ecx

	movl $0x00, LAPIC_EOI_REG	# Indicate completion of servicing the
					# interrupt to LAPIC next

	# Decrement ISR counter and restore general purpose registers.
	ASM_LOCK_PREFIX subl $1, __in_isr
	popa
	iret
.endfunc

# Switches from one task to another if ready.
# __schedule triggers software interrupt ISH_TS_VECTOR, which is handled by
# __switchto
.align 4
.func __switchto
__switchto:

	# Save current task
	pusha
	ASM_LOCK_PREFIX addl $1, __in_isr

	# __schedule() copies 'resched' to %ecx and 'desched' to %edx before
	# triggering ISH_TS_VECTOR
	#
	# Push %ecx and %edx into stack to pass them as function parameters
	# to switch_handler(desched, resched). After call, we clean up stack
	# pointer. Note, we do this now before task_start_irq has a chance
	# to clobber these caller-saved registers.
	irq_handler_common %edx %ecx irq_handler_switch

	# Indicate completion of servicing the interrupt to LAPIC.
	# No IOAPIC EOI needed as this is SW triggered.
	movl $0x00, LAPIC_EOI_REG

	# Decrement ISR counter and restore general purpose registers.
	ASM_LOCK_PREFIX subl $1, __in_isr
	popa
	iret
.endfunc
