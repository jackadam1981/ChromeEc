/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ISH minute-IA CPU initialization
 */

#include "config.h"
#include "interrupts.h"

.section .data
__idt_ptr:
	.word 2047	# Table size in bytes, count from 0
			# (8N - 1). N = 256 - the number of vectors
	.long __idt	# Base address of IDT


.section .init
.code32

# The .init section is mapped to linear address 0xFF000000, the SRAM start.
# ISH BUP (bring-up) downloads ISH FW to ISH SRAM and jumps to start address.
.global reset

# Entry point - core is already set to 32-bit linear mode by ISH ROM
reset:
	# Clear .bss section.
	xorl %eax, %eax
	cld
	movl $__bss_start, %edi
	movl $__bss_size_words, %ecx
	rep stosl

	# System stack is within .bss, already cleared
	movl $stack_end, %esp

	leal __idt, %ebx
	leal default_int_handler, %edx
	mov $NUM_VECTORS, %ecx
iloop:
	## Set up Interrupt Descriptor Table ##

	# ISR address (bits 15:0)
	movl %edx, %eax
	andl $0xffff, %eax
	mov %ax, (%ebx)
	add $2, %ebx

	# CS register (bits 31:16)
	mov %cs, (%ebx)
	add $2, %ebx

	# Unused (bits 39:32)
	movb $0, (%ebx)
	add $1, %ebx

	# IDT flags - (bits 47:40, gate type, segment, dpl, present)
	movb $IDT_FLAGS, %al
	movb %al, (%ebx)
	add $1, %ebx

	# ISR address (bits 63:48)
	movl %edx, %eax
	shrl $16, %eax
	mov %ax, (%ebx)
	add $2, %ebx

	# Loop for 256 entries
	loop iloop

	# Load IDT
	lidt __idt_ptr

	/* Jump to C code */
	jmp main

/* Reserve space for system stack */
.section .bss.system_stack
stack_start:
.space CONFIG_STACK_SIZE, 0
stack_end:
.global stack_end
