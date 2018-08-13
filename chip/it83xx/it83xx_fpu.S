/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * DLMB register = 0x80189:
 * Disable all interrupts and switching CPU's
 * ALU (Arithmetic Logic Unit) to floating point operation mode.
 * (IEEE standard 754 floating point)
 *
 * DLMB register = 0x80009:
 * Restore interrupts and ALU.
 */
	.text
	.align	2
	.global	__addsf3
	.type	__addsf3, @function
__addsf3:
	sethi  $r2, 0x80        /* r2 = 0x80000 */
	addi   $r3, $r2, 0x189  /* r3 = 0x80189 */
	addi45 $r2, 0x9         /* r2 = 0x80009 */
	mtsr   $r3, $dlmb       /* dlmb = 0x80189 */
	dsb
	/* Floating-point addition single-precision */
	add45  $r0, $r1
	mtsr   $r2, $dlmb       /* dlmb = 0x80009 */
	dsb
	ret5   $lp
	.size	__addsf3, .-__addsf3

	.text
	.align	2
	.global	__subsf3
	.type	__subsf3, @function
__subsf3:
	sethi  $r2, 0x80        /* r2 = 0x80000 */
	addi   $r3, $r2, 0x189  /* r3 = 0x80189 */
	addi45 $r2, 0x9         /* r2 = 0x80009 */
	mtsr   $r3, $dlmb       /* dlmb = 0x80189 */
	dsb
	/* Floating-point subtraction single-precision */
	sub45  $r0, $r1
	mtsr   $r2, $dlmb       /* dlmb = 0x80009 */
	dsb
	ret5   $lp
	.size	__subsf3, .-__subsf3

	.text
	.align	2
	.global	__mulsf3
	.type	__mulsf3, @function
__mulsf3:
#if 0
	sethi  $r2, 0x80        /* r2 = 0x80000 */
	addi   $r3, $r2, 0x189  /* r3 = 0x80189 */
	addi45 $r2, 0x9         /* r2 = 0x80009 */
	mtsr   $r3, $dlmb       /* dlmb = 0x80189 */
	dsb
#if 1
	/* Floating-point multiplication single-precision */
	mul33  $r0, $r1
#else
	slli $r5, $r0, 1
	beqz $r5, __zero
	slli $r5, $r1, 1
	beqz $r5, __zero
	mov55 $r5, $r0
	sethi $r0, #0x3f800
	divsr $r0,$r0,$r0,$r1
	mov55 $r1, $r0
	mov55 $r0, $r5
	slli $r5, $r1, 1
	beqz $r5, __zero
	divsr $r0,$r0,$r0,$r1
	b __ret
__zero:
	movi	$r0, #0
__ret:
#endif
	mtsr   $r2, $dlmb       /* dlmb = 0x80009 */
	dsb
	ret5   $lp
#else
#define SIGN	$r3
#define EXPOA	$r4
#define MANTA	$r5
#define VALUA	$r6
#define EXPOB	$r7
#define MANTB	$r8
#define VALUB	$r9
#define SPROD	$r10
        /* save r6-r10 */
	smw.adm	$r6, [$sp], $r10, 0
	/* sign(A xor B) */
	xor SPROD, $r1, $r0
	move SIGN, #0x80000000
	and SPROD, SPROD, SIGN

	slli    VALUA, $r0, 1		! A<<1
	slli	VALUB, $r1, 1		! B<<1

	srli	EXPOA, VALUA, 24	! exponent(A)
	srli	EXPOB, VALUB, 24	! exponent(B)

	slli	MANTA, VALUA, 7		! mantissa(A)<<8
	slli	MANTB, VALUB, 7		! mantissa(B)<<8

	//beqz	EXPOA, .LFzeroAexp	! exponent(A) is zero, goto .LFzeroAexp
	beqz	VALUA, .LFzeroA	        ! exponent(A) and mantissa (A) are zero
	beqc	EXPOA, 0xff, .LFinfnanA	! A is inf or NaN, goto .LFinfnanA
	//or	MANTA, MANTA, SIGN
.LFlab1:
	//beqz	EXPOB, .LFzeroB		! exponent(B) is zero, goto .LFzeroB
	beqz	VALUB, .LFzeroB	        ! exponent(B) and mantissa (B) are zero
	beqc	EXPOB, 0xff, .LFinfnanB	! B is inf or NaN, goto .LFinfnanB
	//or	MANTB, MANTB, SIGN

	sethi  $r2, 0x80        /* r2 = 0x80000 */
	addi   $r3, $r2, 0x189  /* r3 = 0x80189 */
	addi45 $r2, 0x9         /* r2 = 0x80009 */
	mtsr   $r3, $dlmb       /* dlmb = 0x80189 */
	dsb
	mov55 $r5, $r0
	sethi $r0, #0x3f800
	divsr $r0,$r0,$r0,$r1
	mov55 $r1, $r0
	mov55 $r0, $r5
	slli $r5, $r1, 1
	beqz $r5, .LFzero
	divsr $r0,$r0,$r0,$r1
	mtsr   $r2, $dlmb       /* dlmb = 0x80009 */
	dsb
	b	.LFret
.LFpack:
	or	$r0, $r0, SPROD
.LFret:
        /* restore r6-r10 */
	lmw.bim	$r6, [$sp], $r10, 0
	ret5	$lp

	! ---------------------------------------------------------------------
	! exponent(A) is 0x00
	! ---------------------------------------------------------------------
//.LFzeroAexp:
//	bnez	MANTA, .LFloopA2	! A is denorm

	! A is zero
.LFzeroA:
	beqc	EXPOB, 0xff, .LFnan	! zero * inf = zero * NaN = NaN
.LFzero:
	move	$r0, SPROD
	b	.LFret

	! ---------------------------------------------------------------------
	! exponent(A) is 0xff
	! ---------------------------------------------------------------------
.LFinfnanA:
	bne	MANTA, SIGN, .LFnan	! A is NaN: NaN * B = NaN

	! A is inf
	beqz	VALUB, .LFnan		! B is zero: inf * zero = NaN
	bnec	EXPOB, 0xff, .LFinf	! B is finite: inf * B = inf

	! ---------------------------------------------------------------------
	! exponent(B) is 0xff
	! ---------------------------------------------------------------------
.LFinfnanB:
	bne	MANTB, SIGN, .LFnan	! B is NaN: A * NaN = NaN

.LFinf:
	move	$r0, #0x7f800000
	b	.LFpack

	! ---------------------------------------------------------------------
	! exponent(B) is 0x00
	! ---------------------------------------------------------------------
.LFzeroB:
	//bnez	MANTB, .LFloopB2	! B is denorm
	b	.LFzero			! B is zero
.LFnan:
	move	$r0, #0xffc00000
	b	.LFret
#endif
	.size	__mulsf3, .-__mulsf3

	.text
	.align	2
	.global	__divsf3
	.type	__divsf3, @function
__divsf3:
	sethi  $r2, 0x80        /* r2 = 0x80000 */
	addi   $r3, $r2, 0x189  /* r3 = 0x80189 */
	addi45 $r2, 0x9         /* r2 = 0x80009 */
	mtsr   $r3, $dlmb       /* dlmb = 0x80189 */
	dsb
	/* Floating-point division single-precision */
	divsr  $r0,$r0,$r0,$r1
	mtsr   $r2, $dlmb       /* dlmb = 0x80009 */
	dsb
	ret5   $lp
	.size	__divsf3, .-__divsf3
