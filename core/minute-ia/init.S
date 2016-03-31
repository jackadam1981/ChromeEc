/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ISH minute-IA CPU initialization
 */

#include "config.h"

/* Seems 4kB of stack should be more then enough for ROM's simple tasks. */
#define STACK_SIZE			4096

.section .text.vecttable

.macro vector name
.long \name\()_handler
.weak \name\()_handler
.set \name\()_handler, default_int_handler
.endm

.macro vector_irq number
.if \number < CONFIG_IRQ_COUNT
vector irq_\()\number
.endif
.endm

vectors:
vector divide_error		# 0. Divide error
vector dbg_exception		# 1. Debug exception
vector nmi			# 2. NMI handler
vector breakpoint		# 3. Breakpoint
vector overflow			# 4. Overflow
vector bound_exceeded		# 5
vector invalid_opcode		# 6
vector no_device		# 7
vector double_fault		# 8
vector fp_overrun		# 9
vector invalid_tss		# 10
vector no_segment		# 11
vector stack_seg_fault		# 12
vector gpf			# 13
vector page_fault		# 14
.long 0				# 15. reserved
vector x87_fpu_fault		# 16
vector alignment		# 17
vector mce			# 18
vector simd_fault		# 19
vector virt_fault		# 20
.long 0				# 21. reserved
.long 0				# 22. reserved
.long 0				# 23. reserved
.long 0				# 24. reserved
.long 0				# 25. reserved
.long 0				# 26. reserved
.long 0				# 27. reserved
.long 0				# 28. reserved
.long 0				# 29. reserved
.long 0				# 30. reserved
.long 0				# 31. reserved
vector_irq 32                   # vector  32 handler
vector_irq 33                   # vector  33 handler
vector_irq 34                   # vector  34 handler
vector_irq 35                   # vector  35 handler
vector_irq 36                   # vector  36 handler
vector_irq 37                     # vector  37 handler
vector_irq 38                     # vector  38 handler
vector_irq 39                     # vector  39 handler
vector_irq 40                     # vector  40 handler
vector_irq 41                     # vector  41 handler
vector_irq 42                     # vector  42 handler
vector_irq 43                     # vector  43 handler
vector_irq 44                     # vector  44 handler
vector_irq 45                     # vector  45 handler
vector_irq 46                     # vector  46 handler
vector_irq 47                     # vector  47 handler
vector_irq 48                     # vector  48 handler
vector_irq 49                     # vector  49 handler
vector_irq 50                     # vector  50 handler
vector_irq 51                     # vector  51 handler
vector_irq 52                     # vector  52 handler
vector_irq 53                     # vector  53 handler
vector_irq 54                     # vector  54 handler
vector_irq 55                     # vector  55 handler
vector_irq 56                     # vector  56 handler
vector_irq 57                     # vector  57 handler
vector_irq 58                     # vector  58 handler
vector_irq 59                     # vector  59 handler
vector_irq 60                     # vector  60 handler
vector_irq 61                     # vector  61 handler
vector_irq 62                     # vector  62 handler
vector_irq 63                     # vector  63 handler
vector_irq 64                     # vector  64 handler
vector_irq 65                     # vector  65 handler
vector_irq 66                     # vector  66 handler
vector_irq 67                     # vector  67 handler
vector_irq 68                     # vector  68 handler
vector_irq 69                     # vector  69 handler
vector_irq 70                     # vector  70 handler
vector_irq 71                     # vector  71 handler
vector_irq 72                     # vector  72 handler
vector_irq 73                     # vector  73 handler
vector_irq 74                     # vector  74 handler
vector_irq 75                     # vector  75 handler
vector_irq 76                     # vector  76 handler
vector_irq 77                     # vector  77 handler
vector_irq 78                     # vector  78 handler
vector_irq 79                     # vector  79 handler
vector_irq 80                     # vector  80 handler
vector_irq 81                     # vector  81 handler
vector_irq 82                     # vector  82 handler
vector_irq 83                     # vector  83 handler
vector_irq 84                     # vector  84 handler
vector_irq 85                     # vector  85 handler
vector_irq 86                     # vector  86 handler
vector_irq 87                     # vector  87 handler
vector_irq 88                     # vector  88 handler
vector_irq 89                     # vector  89 handler
vector_irq 90                     # vector  90 handler
vector_irq 91                     # vector  91 handler
vector_irq 92                     # vector  92 handler
vector_irq 93                     # vector  93 handler
vector_irq 94                     # vector  94 handler
vector_irq 95                     # vector  95 handler
vector_irq 96                     # vector  96 handler
vector_irq 97                     # vector  97 handler
vector_irq 98                     # vector  98 handler
vector_irq 99                     # vector  99 handler
vector_irq 100                    # vector 100 handler
vector_irq 101                    # vector 101 handler
vector_irq 102                    # vector 102 handler
vector_irq 103                    # vector 103 handler
vector_irq 104                    # vector 104 handler
vector_irq 105                    # vector 105 handler
vector_irq 106                    # vector 106 handler
vector_irq 107                    # vector 107 handler
vector_irq 108                    # vector 108 handler
vector_irq 109                    # vector 109 handler
vector_irq 110                    # vector 110 handler
vector_irq 111                    # vector 111 handler
vector_irq 112                    # vector 112 handler
vector_irq 113                    # vector 113 handler
vector_irq 114                    # vector 114 handler
vector_irq 115                    # vector 115 handler
vector_irq 116                    # vector 116 handler
vector_irq 117                    # vector 117 handler
vector_irq 118                    # vector 118 handler
vector_irq 119                    # vector 119 handler
vector_irq 120                    # vector 120 handler
vector_irq 121                    # vector 121 handler
vector_irq 122                    # vector 122 handler
vector_irq 123                    # vector 123 handler
vector_irq 124                    # vector 124 handler
vector_irq 125                    # vector 125 handler
vector_irq 126                    # vector 126 handler
vector_irq 127                    # vector 127 handler
vector_irq 128                    # vector 128 handler
vector_irq 129                    # vector 129 handler
vector_irq 130                    # vector 130 handler
vector_irq 131                    # vector 131 handler
vector_irq 132                    # vector 132 handler
vector_irq 133                    # vector 133 handler
vector_irq 134                    # vector 134 handler
vector_irq 135                    # vector 135 handler
vector_irq 136                    # vector 136 handler
vector_irq 137                    # vector 137 handler
vector_irq 138                    # vector 138 handler
vector_irq 139                    # vector 139 handler
vector_irq 140                    # vector 140 handler
vector_irq 141                    # vector 141 handler
vector_irq 142                    # vector 142 handler
vector_irq 143                    # vector 143 handler
vector_irq 144                    # vector 144 handler
vector_irq 145                    # vector 145 handler
vector_irq 146                    # vector 146 handler
vector_irq 147                    # vector 147 handler
vector_irq 148                    # vector 148 handler
vector_irq 149                    # vector 149 handler
vector_irq 150                    # vector 150 handler
vector_irq 151                    # vector 151 handler
vector_irq 152                    # vector 152 handler
vector_irq 153                    # vector 153 handler
vector_irq 154                    # vector 154 handler
vector_irq 155                    # vector 155 handler
vector_irq 156                    # vector 156 handler
vector_irq 157                    # vector 157 handler
vector_irq 158                    # vector 158 handler
vector_irq 159                    # vector 159 handler
vector_irq 160                    # vector 160 handler
vector_irq 161                    # vector 161 handler
vector_irq 162                    # vector 162 handler
vector_irq 163                    # vector 163 handler
vector_irq 164                    # vector 164 handler
vector_irq 165                    # vector 165 handler
vector_irq 166                    # vector 166 handler
vector_irq 167                    # vector 167 handler
vector_irq 168                    # vector 168 handler
vector_irq 169                    # vector 169 handler
vector_irq 170                    # vector 170 handler
vector_irq 171                    # vector 171 handler
vector_irq 172                    # vector 172 handler
vector_irq 173                    # vector 173 handler
vector_irq 174                    # vector 174 handler
vector_irq 175                    # vector 175 handler
vector_irq 176                    # vector 176 handler
vector_irq 177                    # vector 177 handler
vector_irq 178                    # vector 178 handler
vector_irq 179                    # vector 179 handler
vector_irq 180                    # vector 180 handler
vector_irq 181                    # vector 181 handler
vector_irq 182                    # vector 182 handler
vector_irq 183                    # vector 183 handler
vector_irq 184                    # vector 184 handler
vector_irq 185                    # vector 185 handler
vector_irq 186                    # vector 186 handler
vector_irq 187                    # vector 187 handler
vector_irq 188                    # vector 188 handler
vector_irq 189                    # vector 189 handler
vector_irq 190                    # vector 190 handler
vector_irq 191                    # vector 191 handler
vector_irq 192                    # vector 192 handler
vector_irq 193                    # vector 193 handler
vector_irq 194                    # vector 194 handler
vector_irq 195                    # vector 195 handler
vector_irq 196                    # vector 196 handler
vector_irq 197                    # vector 197 handler
vector_irq 198                    # vector 198 handler
vector_irq 199                    # vector 199 handler
vector_irq 200                    # vector 200 handler
vector_irq 201                    # vector 201 handler
vector_irq 202                    # vector 202 handler
vector_irq 203                    # vector 203 handler
vector_irq 204                    # vector 204 handler
vector_irq 205                    # vector 205 handler
vector_irq 206                    # vector 206 handler
vector_irq 207                    # vector 207 handler
vector_irq 208                    # vector 208 handler
vector_irq 209                    # vector 209 handler
vector_irq 210                    # vector 210 handler
vector_irq 211                    # vector 211 handler
vector_irq 212                    # vector 212 handler
vector_irq 213                    # vector 213 handler
vector_irq 214                    # vector 214 handler
vector_irq 215                    # vector 215 handler
vector_irq 216                    # vector 216 handler
vector_irq 217                    # vector 217 handler
vector_irq 218                    # vector 218 handler
vector_irq 219                    # vector 219 handler
vector_irq 220                    # vector 220 handler
vector_irq 221                    # vector 221 handler
vector_irq 222                    # vector 222 handler
vector_irq 223                    # vector 223 handler
vector_irq 224                    # vector 224 handler
vector_irq 225                    # vector 225 handler
vector_irq 226                    # vector 226 handler
vector_irq 227                    # vector 227 handler
vector_irq 228                    # vector 228 handler
vector_irq 229                    # vector 229 handler
vector_irq 230                    # vector 230 handler
vector_irq 231                    # vector 231 handler
vector_irq 232                    # vector 232 handler
vector_irq 233                    # vector 233 handler
vector_irq 234                    # vector 234 handler
vector_irq 235                    # vector 235 handler
vector_irq 236                    # vector 236 handler
vector_irq 237                    # vector 237 handler
vector_irq 238                    # vector 238 handler
vector_irq 239                    # vector 239 handler
vector_irq 240                    # vector 240 handler
vector_irq 241                    # vector 241 handler
vector_irq 242                    # vector 242 handler
vector_irq 243                    # vector 243 handler
vector_irq 244                    # vector 244 handler
vector_irq 245                    # vector 245 handler
vector_irq 246                    # vector 246 handler
vector_irq 247                    # vector 247 handler
vector_irq 248                    # vector 248 handler
vector_irq 249                    # vector 249 handler
vector_irq 250                    # vector 250 handler
vector_irq 251                    # vector 251 handler
vector_irq 252                    # vector 252 handler
vector_irq 253                    # vector 253 handler
vector_irq 254                    # vector 254 handler
vector_irq 255                    # vector 255 handler

.section .init
.code32

.global reset

/* Entry point - core is in 32-bit mode due to ISH ROM and BUP */
reset:
	/* Clear .bss section. */
	xorl %eax, %eax
	movl $__bss_start, %edi
	movl $__bss_size_words, %ecx
	rep stosl

	/* Initialize stack space and stack pointer. */
	movl $0x5a5a5a5a, %eax
	movl $STACK_SIZE / 4, %ecx
	rep stosl
	movl %edi, %esp

	/* Set up IDT */
	leal __idt, %ebx
	movl vectors, %edx
	mov $256, %ecx
iloop:
	cmpl $224, %ecx
	jl def_vector
	leal (%edx), %eax
	jmp sys_vector

def_vector:
	leal default_int_handler, %edx
	mov %edx, %eax
sys_vector:
	/* ISR low - 2 bytes */
	andl $0xffff, %eax
	mov %ax, (%ebx)
	add $2, %ebx
	mov %cs, (%ebx)
	add $2, %ebx

	movb $0, (%ebx)
	add $1, %ebx

	movb $0x8e, (%ebx)
	add $1, %ebx

	/* ISR high - 2 bytes */
	movl %edx, %eax
	shrl $16, %eax
	mov %ax, (%ebx)
	add $2, %ebx

	#add $4, %edx - commented for now
	loop iloop

	leal __idt_ptr, %eax
	mov $2047, %bx
	mov %bx, (%eax)
	add $2, %eax
	leal __idt, %ebx
	movl %ebx, (%eax)

	lidt __idt_ptr
	/* Jump to C code */
	jmp main

.align 2
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

/* Dummy functions to avoid linker complaints */
.global __aeabi_unwind_cpp_pr0
.global __aeabi_unwind_cpp_pr1
.global __aeabi_unwind_cpp_pr2
__aeabi_unwind_cpp_pr0:
__aeabi_unwind_cpp_pr1:
__aeabi_unwind_cpp_pr2:

/* Reserve space for system stack */
.section .bss.system_stack
stack_start:
.space CONFIG_STACK_SIZE, 0
stack_end:
.global stack_end
