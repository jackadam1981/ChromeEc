/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Context switching
 */

#include "config.h"

.text

/**
 * Task context switching
 *
 * Change the task scheduled after returning from an interruption.
 *
 * This function must be called in interrupt context.
 *
 * Save the registers of the current task below the interrupt context on
 * its task, then restore the live registers of the next task and set the
 * process stack pointer to the new stack.
 *
 * the structure of the saved context on the stack is :
 * ra, a0-a7, t0-t6 (caller saved) ,  s0-s11 (callee saved), mepc
 * interrupt entry frame          <|> additional registers
 *
 */
.global __switch_task
__switch_task:
	/* get the (new) highest priority task pointer in a0 */
	jal next_sched_task
	/* pointer to the current task (which are switching from) */
	la t1, current_task
	lw t0, 0(t1)
	/* reset the re-scheduling request */
	la t2, need_resched
	sw zero, 0(t2)
	/* Nothing to do: let's return to keep the same task scheduled */
	beq a0, t0, __irq_exit
	/* save our new scheduled task  */
	sw a0, 0(t1)
        /* restore current process stack pointer */
        csrr sp, mscratch
        /* get the task program counter saved at exception entry */
	csrr t5, mepc
	/* save s0-s11 and program counter on the current process stack */
        sw t5, -13*4(sp)
        sw s11, -12*4(sp)
        sw s10, -11*4(sp)
        sw s9, -10*4(sp)
        sw s8, -9*4(sp)
        sw s7, -8*4(sp)
        sw s6, -7*4(sp)
        sw s5, -6*4(sp)
        sw s4, -5*4(sp)
        sw s3, -4*4(sp)
        sw s2, -3*4(sp)
        sw s1, -2*4(sp)
        sw s0, -1*4(sp)
        addi sp, sp, -13*4
        /* save the task stack pointer in its context */
        sw sp, 0(t0)
        /* get the new scheduled task stack pointer */
        lw sp, 0(a0)
        /* restore program counter from the next stack context */
        lw t0, 0*4(sp)
        csrw mepc, t0
        /* restore registers from the next stack context */
        lw s11, 1*4(sp)
        lw s10, 2*4(sp)
        lw s9, 3*4(sp)
        lw s8, 4*4(sp)
        lw s7, 5*4(sp)
        lw s6, 6*4(sp)
        lw s5, 7*4(sp)
        lw s4, 8*4(sp)
        lw s3, 9*4(sp)
        lw s2, 10*4(sp)
        lw s1, 11*4(sp)
        lw s0, 12*4(sp)
        addi sp, sp, 13*4
        /*
         * save sp to scratch register and switch to system stack.
         * __irq_exit will restore sp from scratch register again before mret.
         */
        csrw mscratch, sp
        la sp, stack_end
        j __irq_exit

/**
 * Start the task scheduling.
 */
.global __task_start
__task_start:
	csrci mstatus, 0x8
	/* area used as dummy thread stack for the first switch */
	la a3, scratchpad
	li a4, 1
	li a2, 0    /* system call 3rd parameter : not an IRQ emulation */
	li a1, 0    /* system call 2nd parameter : re-schedule nothing */
	li a0, 0    /* system call 1st parameter : de-schedule nothing */
	/* put the dummy stack pointer at the top of the stack in scratchpad */
	addi sp, a3, 4 * 29
	/* we are ready to re-schedule */
	la t0, need_resched
	sw a4, 0(t0)
        la t0, start_called
        sw a4, 0(t0)
        csrsi mstatus, 0x8
	/* trigger scheduling to execute the task with the highest priority */
	ecall
	/* we should never return here */
	j .
