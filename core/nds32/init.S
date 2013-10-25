/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * N8 CPU initialization
 */

#include "config.h"

.section .text.vecttable

.macro vector name
j \name\()_handler
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
j reset		/* reset / NMI */
vector tlb_fill	/* TLB fill */
vector no_pte	/* PTE not present */
vector tlb_misc	/* TLB misc */
vector tlb_vlpt	/* TLB VLPT miss */
vector merr	/* Machine error */
vector debug	/* Debug related */
vector gen_exc	/* General exception */
vector syscall	/* Syscall */
vector_irq 0	/* HW  0 */
vector_irq 1	/* HW  1 */
vector_irq 2	/* HW  2 */
vector_irq 3	/* HW  3 */
vector_irq 4	/* HW  4 */
vector_irq 5	/* HW  5 */
vector_irq 6	/* HW  6 */
vector_irq 7	/* HW  7 */
vector_irq 8	/* HW  8 */
vector_irq 9	/* HW  9 */
vector_irq 10	/* HW 10 */
vector_irq 11	/* HW 11 */
vector_irq 12	/* HW 12 */
vector_irq 13	/* HW 13 */
vector_irq 14	/* HW 14 */
vector_irq 15	/* HW 15 */

/* E-flash signature */
.balign 16
.byte 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xB4
.byte 0x85, 0x12, 0x5A, 0x5A, 0xAA, 0xAA, 0x55, 0x55
/* flags: internal oscillator + implicit location */

.text

.global reset
reset:
	/* map/enable the 16kB of DLM at 0x00080000 */
	li   $r0, 0x00080005
	mtsr $r0, $mr7

	/* Clear BSS */
	la   $r0, _bss_start
	lwi  $r1, [$r0]
	la   $r0, _bss_end
	lwi  $r2, [$r0]
	movi $r0, #0
bss_loop:
	swi.bi $r0, [$r1], 4
	bne $r1, $r2, bss_loop

	/* Copy initialized data to DLM */
	la $r0, _data_start
	lwi  $r1, [$r0]
	la $r0, _data_end
	lwi  $r2, [$r0]
	la $r0, _ro_end
	lwi  $r0, [$r0]
data_loop:
	lwi.bi $r3, [$r0], 4
	swi.bi $r3, [$r1], 4
	bne $r1, $r2, data_loop

	/* GP register is used to access .data and .bss */
	la $r0, _gp_init
	lwi $gp, [$r0]

	/* Set stack pointer. */
	la $sp, stack_end

	/* Jump to C routine */
	jal main

	/* That should not return.  If it does, loop forever. */
	j .

.global default_handler
default_handler:
	j .

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
_gp_init:
.long __gp_init

/* Reserve space for system stack */
.section .bss.system_stack
stack_start:
.space CONFIG_STACK_SIZE, 0
stack_end:
.global stack_end

