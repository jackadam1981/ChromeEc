/* Copyright 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Cortex-M CPU initialization
 */

#include "config.h"
#include "reset_ram_fill.h"

.text
.syntax unified
.code 16

/*
 * mov r0, #0
 * mov r1, =addr_start
 * mov r2, =addr_end
 * memset r0, r1, r2
 */
.macro memset reg_c, reg_addr_start, reg_addr_end
1:
	cmp \reg_addr_start , \reg_addr_end
	it lt
	strlt \reg_c , [\reg_addr_start], #4
	blt 1b
.endm

/*
 * Jump to label if the codition is triggered and fallthrough, otherwise.
 */
.macro reset_ram_fill_cond_branch cond_reg_ptr, cond_mask, cond_test, label, \
				  reg_tmp0, reg_tmp1, reg_tmp2
	/* Grab cond_reg_ptr's address and then value */
	ldr \reg_tmp0, =\cond_reg_ptr
	ldr \reg_tmp0, [\reg_tmp0]
	/* If cond_reg_ptr is unused skip this macro */
	cmp \reg_tmp0, #(COND_REG_PTR_UNUSED)
	beq 1f
	/* Grab cond_mask and cond_test's address */
	ldr \reg_tmp1, =\cond_mask
	ldr \reg_tmp2, =\cond_test
	/* Grab value *cond_reg_ptr, cond_mask, and cond_test */
	ldr \reg_tmp0, [\reg_tmp0]
	ldr \reg_tmp1, [\reg_tmp1]
	ldr \reg_tmp2, [\reg_tmp2]
	/* (*cond_reg_ptr & cond_mask) == cond_test */
	and \reg_tmp0, \reg_tmp1
	cmp \reg_tmp0, \reg_tmp2
	beq \label
1:
.endm

.global reset
.thumb_func
.type	reset, %function
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

#ifdef CONFIG_RESET_RAM_FILL
	/* Check reset_ram_fill_cond0 */
	reset_ram_fill_cond_branch         \
		reset_ram_fill_cond0_reg,  \
		reset_ram_fill_cond0_mask, \
		reset_ram_fill_cond0_test, \
		reset_ram_fill,            \
		r0, r1, r2

	/* Check reset_ram_fill_cond1 */
	reset_ram_fill_cond_branch         \
		reset_ram_fill_cond1_reg,  \
		reset_ram_fill_cond1_mask, \
		reset_ram_fill_cond1_test, \
		reset_ram_fill,            \
		r0, r1, r2

	/* Check reset_ram_fill_cond2 */
	reset_ram_fill_cond_branch         \
		reset_ram_fill_cond2_reg,  \
		reset_ram_fill_cond2_mask, \
		reset_ram_fill_cond2_test, \
		reset_ram_fill,            \
		r0, r1, r2

	b bss_init

reset_ram_fill:
	/* Clear all RAM */
	ptr        .req r0
	ptr_end    .req r1
	zero       .req r2
	addr_start .req r3
	addr_end   .req r4

	/* Set ptr to the first entry of ram_clear_table */
	ldr ptr, =ram_table
	/* Set ptr_end to one entry past the end of ram_clear_table */
	ldr ptr_end, =ram_table_size
	ldr ptr_end, [ptr_end]
	add ptr_end, ptr

reset_ram_fill_loop:
	cmp ptr, ptr_end
	bge bss_init_skip
	ldrd addr_start, addr_end, [ptr], #8
	mov zero, #0
	memset zero, addr_start, addr_end
	b reset_ram_fill_loop

	.unreq zero
	.unreq addr_end
	.unreq addr_start
	.unreq ptr_end
	.unreq ptr
#endif /* CONFIG_RESET_RAM_FILL */

	/* Clear BSS */
bss_init:
	mov r0, #0
	ldr r1,_bss_start
	ldr r2,_bss_end
	memset r0, r1, r2
bss_init_skip:

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
.size	reset, .-reset

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

