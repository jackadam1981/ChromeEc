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

# desched resched irq are all inputs and should be set to registers or immediate
# values
.macro irq_handler_common desched resched irq
	pusha
	ASM_LOCK_PREFIX addl $1, __in_isr

	# __schedule() copies 'resched' to %ecx and 'desched' to %edx before
	movl %esp, %eax
	movl $stack_end, %esp		# use system stack
	push %eax			# push sp of preempted context

	# Push resched and desched on stack to pass them as function parameters
	# to switch_handler(desched, resched). After call, we clean up stack
	# pointer. Note, we do this now before call_irq_service_routine has a
	# chance to clobber these caller-saved registers.
	push \resched
	push \desched

	push \irq
#ifdef CONFIG_TASK_PROFILING
	call task_start_irq_handler
#endif
	# Leave IRQ on stack for handler
	call call_irq_service_routine
	addl $0x04, %esp

	# Call switch_handler(desched, resched).
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

	# Indicate completion of servicing the interrupt to LAPIC.
	# No IOAPIC EOI needed as this is SW triggered.
	movl $0x00, LAPIC_EOI_REG

	# Decrement ISR counter and restore general purpose registers.
	ASM_LOCK_PREFIX subl $1, __in_isr
	popa
	iret
.endm

.align 4
.func sw_irq_handler
sw_irq_handler:
	# Call sw irq handler with irq number(%ecx) from task_trigger_irq.
	# Pass 0 for both desched and resched since we don't need to deschedule
	# our current task (and idle task can always be rescheduled)
	irq_handler_common $0 $0 %ecx
.endfunc


# Switches from one task to another if ready.
# __schedule triggers software interrupt ISH_TS_VECTOR, which is handled by
# __switchto
.align 4
.func __switchto
__switchto:
	# __schedule() copies 'resched' to %ecx and 'desched' to %edx before
	# triggering ISH_TS_VECTOR.
	# Call sw_irq with an invalid IRQ so it will skip calling a routine
	irq_handler_common %edx %ecx $CONFIG_IRQ_COUNT
.endfunc
