/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_TASK_DEFS_H
#define __CROS_EC_TASK_DEFS_H

typedef union {
	struct {
		/*
		 * Note that sp must be the first element in the task struct
		 * for __switchto() to work.
		 */
		uint32_t sp;	/* Saved stack pointer for context switch */
		uint32_t events;	/* Bitmaps of received events */
		uint64_t runtime;	/* Time spent in task */
		uint32_t *stack;	/* Start of stack */
	};
} task_;

int __task_start(int *task_stack_ready);
void __switchto(void);

/* Only the IF bit is set so tasks start with interrupts enabled. */
#define INITIAL_EFLAGS		(0x200UL)
#define LAPIC_ICR_VECTOR	0xC000
#define LAPIC_ICR_REG		0xfee00300
#define LAPIC_ICR_REG_ADDR	REG32(LAPIC_ICR_REG)

#endif	/* __CROS_EC_TASK_DEFS_H */
