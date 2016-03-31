/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ISH minute-IA CPU initialization
 */

#include "config.h"
#include "interrupts.h"

.global __idt

# GDT is loaded by ISH ROM. The FW code retains the same GDT
# and hence the same segment selector
.set code_seg, 0x8

.section .text.vecttable

# Macro that defines an interrupt descriptor
.macro interrupt_descriptor number
	.word def_irq_low	# low 16 bits of default_int_handler
	.word code_seg
	.byte 0
	.byte IDT_FLAGS
	.word def_irq_high	# high 16 bits of default_int_handler
.endm

.align 32
# Static Interrupt descriptor table (vectors 0-255), all vectors initialized
# to default_int_handler. DECLARE_IRQ() remaps the the appropriate handler.
__idt:
interrupt_descriptor 0		# Divide error
interrupt_descriptor 1		# Debug
interrupt_descriptor 2		# NMI interrupt
interrupt_descriptor 3		# Breakpoint
interrupt_descriptor 4		# Overflow
interrupt_descriptor 5		# Bound range exceeded
interrupt_descriptor 6		# Invalid opcode
interrupt_descriptor 7		# Device not available
interrupt_descriptor 8		# Double fault
interrupt_descriptor 8		# Coprocessor overrun
interrupt_descriptor 10		# Invalid TSS
interrupt_descriptor 11		# Segment not present
interrupt_descriptor 12		# Stack segment fault
interrupt_descriptor 13		# General protection
interrupt_descriptor 14		# Page fault
interrupt_descriptor 15
interrupt_descriptor 16		# Floating point error
interrupt_descriptor 17		# Alignment check
interrupt_descriptor 18		# Machine check
interrupt_descriptor 19		# SIMD floating-point exception
interrupt_descriptor 20		# Virtualization exception
interrupt_descriptor 21
interrupt_descriptor 22
interrupt_descriptor 23
interrupt_descriptor 24
interrupt_descriptor 25
interrupt_descriptor 26
interrupt_descriptor 27
interrupt_descriptor 28
interrupt_descriptor 29
interrupt_descriptor 30
interrupt_descriptor 31
interrupt_descriptor 32
interrupt_descriptor 32
interrupt_descriptor 33
interrupt_descriptor 34
interrupt_descriptor 35
interrupt_descriptor 36
interrupt_descriptor 37
interrupt_descriptor 38
interrupt_descriptor 39
interrupt_descriptor 40
interrupt_descriptor 41
interrupt_descriptor 42
interrupt_descriptor 43
interrupt_descriptor 44
interrupt_descriptor 45
interrupt_descriptor 46
interrupt_descriptor 47
interrupt_descriptor 48
interrupt_descriptor 49
interrupt_descriptor 50
interrupt_descriptor 51
interrupt_descriptor 52
interrupt_descriptor 53
interrupt_descriptor 54
interrupt_descriptor 55
interrupt_descriptor 56
interrupt_descriptor 57
interrupt_descriptor 58
interrupt_descriptor 59
interrupt_descriptor 60
interrupt_descriptor 61
interrupt_descriptor 62
interrupt_descriptor 63
interrupt_descriptor 64
interrupt_descriptor 65
interrupt_descriptor 66
interrupt_descriptor 67
interrupt_descriptor 68
interrupt_descriptor 69
interrupt_descriptor 70
interrupt_descriptor 71
interrupt_descriptor 72
interrupt_descriptor 73
interrupt_descriptor 74
interrupt_descriptor 75
interrupt_descriptor 76
interrupt_descriptor 77
interrupt_descriptor 78
interrupt_descriptor 79
interrupt_descriptor 80
interrupt_descriptor 81
interrupt_descriptor 82
interrupt_descriptor 83
interrupt_descriptor 84
interrupt_descriptor 85
interrupt_descriptor 86
interrupt_descriptor 87
interrupt_descriptor 88
interrupt_descriptor 89
interrupt_descriptor 90
interrupt_descriptor 91
interrupt_descriptor 92
interrupt_descriptor 93
interrupt_descriptor 94
interrupt_descriptor 95
interrupt_descriptor 96
interrupt_descriptor 97
interrupt_descriptor 98
interrupt_descriptor 99
interrupt_descriptor 100
interrupt_descriptor 101
interrupt_descriptor 102
interrupt_descriptor 103
interrupt_descriptor 104
interrupt_descriptor 105
interrupt_descriptor 106
interrupt_descriptor 107
interrupt_descriptor 108
interrupt_descriptor 109
interrupt_descriptor 110
interrupt_descriptor 111
interrupt_descriptor 112
interrupt_descriptor 113
interrupt_descriptor 114
interrupt_descriptor 115
interrupt_descriptor 116
interrupt_descriptor 117
interrupt_descriptor 118
interrupt_descriptor 119
interrupt_descriptor 120
interrupt_descriptor 121
interrupt_descriptor 122
interrupt_descriptor 123
interrupt_descriptor 124
interrupt_descriptor 125
interrupt_descriptor 126
interrupt_descriptor 127
interrupt_descriptor 128
interrupt_descriptor 129
interrupt_descriptor 130
interrupt_descriptor 131
interrupt_descriptor 132
interrupt_descriptor 133
interrupt_descriptor 134
interrupt_descriptor 135
interrupt_descriptor 136
interrupt_descriptor 137
interrupt_descriptor 138
interrupt_descriptor 139
interrupt_descriptor 140
interrupt_descriptor 141
interrupt_descriptor 142
interrupt_descriptor 143
interrupt_descriptor 144
interrupt_descriptor 145
interrupt_descriptor 146
interrupt_descriptor 147
interrupt_descriptor 148
interrupt_descriptor 149
interrupt_descriptor 150
interrupt_descriptor 151
interrupt_descriptor 152
interrupt_descriptor 153
interrupt_descriptor 154
interrupt_descriptor 155
interrupt_descriptor 156
interrupt_descriptor 157
interrupt_descriptor 158
interrupt_descriptor 159
interrupt_descriptor 160
interrupt_descriptor 161
interrupt_descriptor 162
interrupt_descriptor 163
interrupt_descriptor 164
interrupt_descriptor 165
interrupt_descriptor 166
interrupt_descriptor 167
interrupt_descriptor 168
interrupt_descriptor 169
interrupt_descriptor 170
interrupt_descriptor 171
interrupt_descriptor 172
interrupt_descriptor 173
interrupt_descriptor 174
interrupt_descriptor 175
interrupt_descriptor 176
interrupt_descriptor 177
interrupt_descriptor 178
interrupt_descriptor 179
interrupt_descriptor 180
interrupt_descriptor 181
interrupt_descriptor 182
interrupt_descriptor 183
interrupt_descriptor 184
interrupt_descriptor 185
interrupt_descriptor 186
interrupt_descriptor 187
interrupt_descriptor 188
interrupt_descriptor 189
interrupt_descriptor 190
interrupt_descriptor 191
interrupt_descriptor 192
interrupt_descriptor 193
interrupt_descriptor 194
interrupt_descriptor 195
interrupt_descriptor 196
interrupt_descriptor 197
interrupt_descriptor 198
interrupt_descriptor 199
interrupt_descriptor 200
interrupt_descriptor 201
interrupt_descriptor 202
interrupt_descriptor 203
interrupt_descriptor 204
interrupt_descriptor 205
interrupt_descriptor 206
interrupt_descriptor 207
interrupt_descriptor 208
interrupt_descriptor 209
interrupt_descriptor 210
interrupt_descriptor 211
interrupt_descriptor 212
interrupt_descriptor 213
interrupt_descriptor 214
interrupt_descriptor 215
interrupt_descriptor 216
interrupt_descriptor 217
interrupt_descriptor 218
interrupt_descriptor 219
interrupt_descriptor 220
interrupt_descriptor 221
interrupt_descriptor 222
interrupt_descriptor 223
interrupt_descriptor 224
interrupt_descriptor 225
interrupt_descriptor 226
interrupt_descriptor 227
interrupt_descriptor 228
interrupt_descriptor 229
interrupt_descriptor 230
interrupt_descriptor 231
interrupt_descriptor 232
interrupt_descriptor 233
interrupt_descriptor 234
interrupt_descriptor 235
interrupt_descriptor 236
interrupt_descriptor 237
interrupt_descriptor 238
interrupt_descriptor 239
interrupt_descriptor 240
interrupt_descriptor 241
interrupt_descriptor 242
interrupt_descriptor 243
interrupt_descriptor 244
interrupt_descriptor 245
interrupt_descriptor 246
interrupt_descriptor 247
interrupt_descriptor 248
interrupt_descriptor 249
interrupt_descriptor 250
interrupt_descriptor 251
interrupt_descriptor 252
interrupt_descriptor 253
interrupt_descriptor 254
interrupt_descriptor 255

#.section .data
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

	# Disabling interrupts initially. It will be enabled when tasks start
	cli

	# Clear .bss section.
	xorl %eax, %eax
	cld
	movl $__bss_start, %edi
	movl $__bss_size_words, %ecx
	rep stosl

	# System stack is within .bss, already cleared
	movl $stack_end, %esp

	# Load IDT
	lidt __idt_ptr

#ifdef CONFIG_FPU
	fninit
#endif

	# Jump to C code
	jmp main

# Reserve space for system stack
.section .bss.system_stack
stack_start:
.space CONFIG_STACK_SIZE, 0
stack_end:
.global stack_end
