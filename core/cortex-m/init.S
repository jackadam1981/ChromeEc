/* Copyright 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Cortex-M CPU initialization
 */

#include "config.h"

.text
.syntax unified
.code 16

/*
 * mov r0, =addr_start
 * mov r1, #0
 * mov r2, =size
 * memset r0, r1, r2
 */
.macro memset reg_addr_start, reg_c, reg_size
	add \reg_size , \reg_addr_start
1:
	cmp \reg_addr_start , \reg_size
	it lt
	strlt \reg_c , [\reg_addr_start], #4
	blt 1b
.endm

.global reset
.thumb_func
reset:
	/*
	 * Ensure we're in privileged mode with main stack.  Necessary if
	 * we've jumped directly here from another image after task_start().
	 */
#ifdef CONFIG_FPU
	mov r0, #(1 << 2)    @ priv. mode / main stack / floating point on
#else
	mov r0, #0           @ priv. mode / main stack / no floating point
#endif
	msr control, r0
	isb                  @ ensure the write is done

	/* Set the vector table on our current code */
	ldr r1, =vectors
	ldr r2, =0xE000ED08   /* VTABLE register in SCB*/
	str r1, [r2]

#ifdef CONFIG_STARTUP_RAM_ZERO
sramclear:
	/* Clear main SRAM bank */
	ptr .req r0
	ptrend .req r1
	addr .req r2
	size .req r3
	zero .req r4

	/* Set ptr to the first entry of ram_clear_table */
	ldr ptr, =ram_clear_table
	/* Set ptrend to one entry past the end of ram_clear_table */
	ldr ptrend, =ram_clear_table_size
	ldr ptrend, [ptrend]
	add ptrend, ptr

sramclear_loop:
	cmp ptr, ptrend
	blt sramclear_finish
	ldr addr, [ptr], #4
	ldr size, [ptr], #4
	mov zero, #0
	memset addr zero size
	b sramclear_loop

	.unreq zero
	.unreq size
	.unreq addr
	.unreq ptrend
	.unreq ptr
sramclear_finish:
#else /* !CONFIG_RAM_CLEAR */
bssclear:
	/* Clear BSS */
	mov r0, #0
	ldr r1,_bss_start
	ldr r2,_bss_end
bss_loop:
	cmp r1, r2
	it lt
	strlt r0, [r1], #4
	blt bss_loop
#endif /* CONFIG_RAM_CLEAR */

	/* Copy initialized data to Internal RAM */
	ldr r0,_data_lma_start
	ldr r1,_data_start
	ldr r2,_data_end
data_loop:
	ldr r3, [r0], #4
	cmp r1, r2
	it lt
	strlt r3, [r1], #4
	blt data_loop

	/*
	 * Set stack pointer.  Already done by Cortex-M hardware, but re-doing
	 * this here allows software to jump directly to the reset vector.
	 */
	ldr r0, =stack_end
	mov sp, r0

#ifdef CONFIG_FPU
	/* Enable FPU */
	/* CPACR is located at address 0xE000ED88 */
	ldr r0, =0xE000ED88
	/* Read CPACR */
	ldr r1, [r0]
	/* Set bits 20-23 to enable CP10 and CP11 coprocessors */
	orr r1, r1, #(0xF << 20)
	/* Write back the modified value to the CPACR */
	str r1, [r0] /* wait for store to complete */
	dsb
	/* reset pipeline now the FPU is enabled */
	isb
#endif /* CONFIG_FPU */

#ifdef CONFIG_DEBUG_DISABLE_WRITE_BUFFER
	/* Disable write buffer used for default memory map accesses */
	ldr r0, =0xE000E008 /* Load address of ACTLR */
	ldr r1, [r0]        /* Read ACTLR */
	orr r1, r1, #2      /* Set DISDEFWBUF bit */
	str r1, [r0]        /* Write back ACTLR */
	dsb                 /* Wait for store to complete */
	isb                 /* Reset pipeline */
#endif /* CONFIG_DEBUG_DISABLE_WRITE_BUFFER */

	/* Jump to C code */
	bl main

	/* That should not return.  If it does, loop forever. */
fini_loop:
	b fini_loop

.align 2
_bss_start:
.long __bss_start
_bss_end:
.long __bss_end
_data_start:
.long __data_start
_data_end:
.long __data_end
_data_lma_start:
.long __data_lma_start

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

