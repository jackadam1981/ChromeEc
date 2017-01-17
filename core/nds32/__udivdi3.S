/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * __udivdi3.S: unsigned 64 bit division
 */

	.text
	.align	2
	.globl	__udivdi3
	.type	__udivdi3, @function
__udivdi3:
	push25	$r8, 0
	movi	$r4, 0
	bal	__udivmoddi4
	pop25	$r8, 0
	.size	__udivdi3, .-__udivdi3

#define P1H     $r1
#define P1L     $r0
#define P2H     $r3
#define P2L     $r2
#define W6H     $r5
#define W6L     $r4
#define OFFSET_L 0
#define OFFSET_H 4

	.text
	.align	2
	.globl	fudiv_qrnnd
	.type	fudiv_qrnnd, @function
fudiv_qrnnd:
	smw.adm	$r0, [$sp], $r4, 2
	srli	$r7, $r6, 16
	movi	$ta, 65535
	and	$r8, $r6, $ta
	divr	$r9, $r10, $r4, $r7
	and	$r4, $r5, $ta
	slli	$r10, $r10, 16
	srli	$ta, $r5, 16
	or	$r10, $r10, $ta
	mul	$r5, $r9, $r8
	slt	$ta, $r10, $r5
	beqz	$ta, .L2
	add	$r10, $r10, $r6
	addi	$r9, $r9, -1
	slt	$ta, $r10, $r6
	bnez	$ta, .L2
	slt	$ta, $r10, $r5
	beqz	$ta, .L2
	addi	$r9, $r9, -1
	add	$r10, $r10, $r6
.L2:
	sub	$r10, $r10, $r5
	divr	$r7, $r10, $r10, $r7
	slli	$r10, $r10, 16
	mul	$r5, $r8, $r7
	or	$r10, $r4, $r10
	slt	$ta, $r10, $r5
	beqz	$ta, .L5
	add	$r10, $r10, $r6
	addi	$r7, $r7, -1
	slt	$ta, $r10, $r6
	bnez	$ta, .L5
	slt	$ta, $r10, $r5
	beqz	$ta, .L5
	add	$r10, $r10, $r6
	addi	$r7, $r7, -1
.L5:
	sub	$r8, $r10, $r5
	slli	$r9, $r9, 16
	or	$r7, $r9, $r7
	lmw.bim	$r0, [$sp], $r4, 2
	ret
	.size	fudiv_qrnnd, .-fudiv_qrnnd

	.align	2
	.globl	__udivmoddi4
	.type	__udivmoddi4, @function
__udivmoddi4:
	addi	$sp, $sp, -40
	smw.bi	$r6, [$sp], $r10, 10
	move	$fp, $r4
	bnez	P2H, .L9
	slt	$ta, P1H, P2L
	beqz	$ta, .L10
	smw.adm	$r0, [$sp], $r5, 0
	move	$r0, P2L
	bal	__clzsi2
	move	$r7, $r0
	lmw.bim	$r0, [$sp], $r5, 0
	swi	$r7, [$sp+(28)]
	beqz	$r7, .L18
	subri	$r5, $r7, 32
	srl	$r5, P1L, $r5
	sll	$r6, P1H, $r7
	or	P1H, $r6, $r5
	sll	P1L, P1L, $r7
	sll	P2L, P2L, $r7
.L18:
	move	$r4, P1H
	move	$r5, P1L
	move	$r6, P2L
	bal	fudiv_qrnnd
	movi	$r6, 0
	swi	$r7, [$sp+32]
	swi	$r6, [$sp+36]
	move	P1L, $r8
	b	.L19
.L10:
	bnez	P2L, .L20
	movi	$r4, 1
	divr	P2L, $r4, $r4, P2L
.L20:
	smw.adm	$r0, [$sp], $r5, 0
	move	$r0, P2L
	bal	__clzsi2
	move	$r7, $r0
	lmw.bim	$r0, [$sp], $r5, 0
	swi	$r7, [$sp+(28)]
	beqz	$r7, .L28
	subri	$r10, $r7, 32
	srl	$r4, P1L, $r10
	sll	$r5, P1H, $r7
	or	$r5, $r5, $r4
	sll	P2L, P2L, $r7
	sll	P1L, P1L, $r7
	srl	$r4, P1H, $r10
	move	$r6, P2L
	bal	fudiv_qrnnd
	swi	$r7, [$sp+(36)]
	move	P1H, $r8
	move	$r4, $r8
	move	$r5, P1L
	move	$r6, P2L
	b	.L29
.L28:
	sub	P1H, P1H, P2L
	movi	$ta, 1
	swi	$ta, [$sp+(36)]
	move	$r4, P1H
	move	$r5, P1L
	move	$r6, P2L
.L29:
	bal	fudiv_qrnnd
	swi	$r7, [$sp+(32)]
	move	P1L, $r8
.L19:
	beqz	$fp, .L31
	movi	$r5, 0
	lwi	$r7, [$sp+(28)]
	srl	$r4, P1L, $r7
	swi	$r4, [$fp+OFFSET_L]
	swi	$r5, [$fp+OFFSET_H]
	b .L31
.L9:
	slt	$ta, P1H, P2H
	beqz	$ta, .L32
	movi	$r5, 0
	swi	$r5, [$sp+(32)]
	swi	$r5, [$sp+(36)]
	beqz	$fp, .L31
	swi	P1L, [$fp+OFFSET_L]
	swi	P1H, [$fp+OFFSET_H]
	b	.L31
.L32:
	smw.adm	$r0, [$sp], $r5, 0
	move	$r0, P2H
	bal	__clzsi2
	move	$r7, $r0
	lmw.bim	$r0, [$sp], $r5, 0
	swi	$r7, [$sp+(28)]
	beqz	$r7, .L42
	subri	$r10, $r7, 32
	srl	$r5, P2L, $r10
	sll	$r6, P2H, $r7
	or	$r6, $r5, $r6
	move	P2H, $r6
	srl	$r4, P1H, $r10
	srl	$r8, P1L, $r10
	sll	$r9, P1H, $r7
	or	$r5, $r8, $r9
	sll	P2L, P2L, $r7
	sll	P1L, P1L, $r7
	bal	fudiv_qrnnd
	swi	$r7, [$sp+(32)]
	move	P1H, $r8
	move	$r6, $r7
	smw.adm	$r0, [$sp], $r3, 0
	move	P1L, P2L
	move	P2L, $r6
	movi	P1H, 0
	movi	P2H, 0
	bal	__muldi3
	movd44	$r4, $r0
	lmw.bim	$r0, [$sp], $r3, 0
	move	$r8, W6H
	move	$r5, W6L
	slt	$ta, P1H, $r8
	bnez	$ta, .L46
	bne	$r8, P1H, .L45
	slt	$ta, P1L, $r5
	beqz	$ta, .L45
.L46:
	sub	$r4, $r5, P2L
	addi	$r6, $r6, -1
	sub	$r8, $r8, P2H
	swi	$r6, [$sp+(32)]
	slt	$ta, $r5, $r4
	sub	$r8, $r8, $ta
	move	$r5, $r4
.L45:
	movi	$r4, 0
	swi	$r4, [$sp+(36)]
	beqz	$fp, .L31
	sub	$r4, P1H, $r8
	sub	$r6, P1L, $r5
	slt	$ta, P1L, $r6
	sub	P1H, $r4, $ta
	move	P1L, $r6
	lwi	$r7, [$sp+(28)]
	subri	$r10, $r7,32
	sll	$r4, P1H, $r10
	srl	$r5, P1L, $r7
	or	$r6, $r5, $r4
	srl	$r8, P1H, $r7
	swi	$r6, [$fp+OFFSET_L]
	swi	$r8, [$fp+OFFSET_H]
	b	.L31
.L42:
	slt	$ta, P2H, P1H
	bnez	$ta, .L52
	slt	$ta, P1L, P2L
	bnez	$ta, .L51
.L52:
	sub	$r4, P1H, P2H
	sub	$r6, P1L, P2L
	slt	$ta, P1L, $r6
	sub	P1H, $r4, $ta
	move    P1L, $r6
	movi	$r5, 1
	swi	$r5, [$sp+(32)]
	b	.L54
.L51:
	movi	$r5,0
	swi	$r5, [$sp+(32)]
.L54:
	movi	$r5, 0
	swi	$r5, [$sp+(36)]
	beqz	$fp, .L31
	swi	P1L, [$fp+OFFSET_L]
	swi	P1H, [$fp+OFFSET_H]
.L31:
	lwi	P1L, [$sp+(32)]
	lwi	P1H, [$sp+(36)]
	lmw.bim	$r6, [$sp], $r10, 10
	addi	$sp, $sp, 12
	ret
	.size	__udivmoddi4, .-__udivmoddi4
