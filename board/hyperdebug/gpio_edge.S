/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Custom GPIO interrupt handling logic.
 */

#include "config.h"

.text

.syntax unified
.code 16


#define BUF_HEAD_TIME_OFF 0
#define BUF_HEAD_OFF 4
#define BUF_TAIL_OFF 8
#define BUF_SIZE_OFF 12
#define BUF_TAIL_TIME_OFF 16
#define BUF_OVERFLOW_OFF 24
#define BUF_NUM_SIGNALS_OFF 25
#define BUF_SIGNAL_BITS_OFF 26

my_slot:	
cyclic_buffer_header:
	.long 117
gpio_base:
	.long 117
gpio_pin_mask:
	.long 117
gpio_signal:
	.long 117
head_level:
	.long 117
tail_level:
	.byte 117
signal_no:
	.byte 117

	.byte 117
	.byte 117

.global edge_int
.thumb_func
edge_int:
	stmdb   sp!, {r4, r5, r6, lr}
#if 0
	ldr	r0, =#0x42020C00 /* STM32_GPIOD_BASE */
	mov	r1, #0x00010000
	str	r1, [r0, #0x18] /* STM32_GPIO_BSRR */
#endif	
	ldr	r2, gpio_pin_mask
	ldr	r0, =0x4002f400
	str	r2, [r0, #0x0c] /* STM32_EXTI_RPR */
	str	r2, [r0, #0x10] /* STM32_EXTI_FPR */
	ldr	r1, gpio_base
	ldr 	lr, [r1, #0x10] /* STM32_GPIO_IDR(gpio_base) */
	ands	lr, lr, r2
	ite	ne
	strne	r2, [r0, #0x0c] /* STM32_EXTI_RPR */
	streq	r2, [r0, #0x10] /* STM32_EXTI_FPR */

	/* Load current timestamp into r2 */
	mov 	r0, #0x40000000 /* STM32_TIM2_BASE */
	ldr	r2, [r0, #0x24]
	
	ldr	r6, cyclic_buffer_header
	ldr	r0, [r6, #BUF_HEAD_TIME_OFF]
	sub	r1, r2, r0
	str	r2, [r6, #BUF_HEAD_TIME_OFF]
	ldrb	r0, [r6, #BUF_SIGNAL_BITS_OFF]
	lsl	r1, r1, r0
	ldrb	r0, signal_no
	orr	r1, r1, r0

	/*
	* r1: diff
	* r2: buffer_data[0]
	* r3: buffer_data[head]
	* r4: buffer_data[tail]	
	* r5: buffer_data[size]
	* r6: cyclic_buffer_header_t address
	* lr: current logic level
	*/
	add	r2, r6, #32 /* sizeof(cyclic_buffer_header_t) */
	ldr	r3, [r6, #4]
	ldr	r4, [r6, #8]
	ldr	r5, [r6, #12]

	cmp	r1, #0x80
	bhs	encode_multiple_bytes

encode_last_byte:	
	strb	r1, [r3], #1
	cmp	r3, r5
	it	eq
	moveq	r3, r2
	cmp	r3, r4
	beq	overflow

	ldr	r1, head_level
	cmp	r1, lr
	beq	encode_extra_edge
	adr	r0, head_level
	str	lr, [r0]
leave:
	str	r3, [r6, #4]

#if 0
	ldr	r0, =#0x42020C00 /* STM32_GPIOD_BASE */
	mov	r1, #0x00000001
	str	r1, [r0, #0x18] /* STM32_GPIO_BSRR */
#endif
	ldmia.w sp!, {r4, r5, r6, pc}

.ltorg

encode_multiple_bytes:	
	and	r0, r1, #0xFF
	orr	r0, r0, #0x80
	lsr	r1, r1, #7
	strb	r0, [r3], #1
	cmp	r3, r5
	it	eq
	moveq	r3, r2
	cmp	r3, r4
	beq	report_overflow
	cmp	r1, #0x80
	bhs	encode_multiple_bytes
	b	encode_last_byte

encode_extra_edge:
	ldrb	r0, signal_no
	strb	r0, [r3], #1
	cmp	r3, r5
	it	eq
	moveq	r3, r2
	cmp	r3, r4
	bne	leave

report_overflow:	
	ldr	r1, =overflow
	adr	r0, my_slot
	bx	r1

.global edge_int_end
edge_int_end:
	
