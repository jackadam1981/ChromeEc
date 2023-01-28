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

/*
 * Offsets of fields in struct cyclic_buffer_header_t.
 */	
#define BUF_HEAD_TIME_OFF 0
#define BUF_HEAD_OFF 4
#define BUF_TAIL_OFF 8
#define BUF_SIZE_OFF 12
#define BUF_TAIL_TIME_OFF 16
#define BUF_OVERFLOW_OFF 24
#define BUF_NUM_SIGNALS_OFF 25
#define BUF_SIGNAL_BITS_OFF 26
#define BUF_DATA_OFF 32

/*
 * Below is a list of data declaration in flash.  The compiler will generate
 * pc-relative instructions to access these fields from the code below.  The
 * idea being that when this code is copied into monitoring_slot_t[i].code for
 * all 16 array elements, then each copy of the code will access the fields from
 * "its own" index in the array, without any indexing effort.
 */
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
load_pin_mask_location:
	mov	r2, #0x0001
load_pin_mask_location_end:
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
signal_bits_location:	
	lsl	r1, r1, #3
signal_bits_location_end:	
signal_no_location:	
	orr	r1, r1, #7
signal_no_location_end:	

	/*
	* r1: diff
	* r2: buffer_data[0]
	* r3: buffer_data[head]
	* r4: buffer_data[tail]	
	* r5: buffer_data[size]
	* r6: cyclic_buffer_header_t address
	* lr: current logic level
	*/
	add	r2, r6, #BUF_DATA_OFF
	ldr	r3, [r6, #BUF_HEAD_OFF]
	ldr	r4, [r6, #BUF_TAIL_OFF]
	ldr	r5, [r6, #BUF_SIZE_OFF]

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
	str	r3, [r6, #BUF_HEAD_OFF]

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

.data
.global load_pin_mask_replacement
load_pin_mask_replacement:
	.long 16
	.long load_pin_mask_location
	.long load_pin_mask_location_end
	.long load_pin_mask_table
	.long load_pin_mask_table_end

.text
load_pin_mask_table:
	mov	r2, #0x0001
	mov	r2, #0x0002
	mov	r2, #0x0004
	mov	r2, #0x0008
	mov	r2, #0x0010
	mov	r2, #0x0020
	mov	r2, #0x0040
	mov	r2, #0x0080
	mov	r2, #0x0100
	mov	r2, #0x0200
	mov	r2, #0x0400
	mov	r2, #0x0800
	mov	r2, #0x1000
	mov	r2, #0x2000
	mov	r2, #0x4000
	mov	r2, #0x8000
load_pin_mask_table_end:

.data
.global signal_no_replacement
signal_no_replacement:
	.long 16
	.long signal_no_location
	.long signal_no_location_end
	.long signal_no_table
	.long signal_no_table_end

.text
signal_no_table:
	orr	r1, r1, #0
	orr	r1, r1, #1
	orr	r1, r1, #2
	orr	r1, r1, #3
	orr	r1, r1, #4
	orr	r1, r1, #5
	orr	r1, r1, #6
	orr	r1, r1, #7
	orr	r1, r1, #8
	orr	r1, r1, #9
	orr	r1, r1, #10
	orr	r1, r1, #11
	orr	r1, r1, #12
	orr	r1, r1, #13
	orr	r1, r1, #14
	orr	r1, r1, #15
signal_no_table_end:

.data
.global signal_bits_replacement
signal_bits_replacement:
	.long 4
	.long signal_bits_location
	.long signal_bits_location_end
	.long signal_bits_table
	.long signal_bits_table_end

.text
signal_bits_table:
	lsl	r1, r1, #0
	lsl	r1, r1, #1
	lsl	r1, r1, #2
	lsl	r1, r1, #3
signal_bits_table_end:


