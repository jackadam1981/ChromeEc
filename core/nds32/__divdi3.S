/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * __divdi3.S: signed 64 bit division
 */

	.text
	.align	2
	.globl	__negdi2
	.type	__negdi2, @function
__negdi2:
	neg33	$r0, $r0
	movi55	$r2, 0
	slt	$r2, $r2, $r0
	neg33	$r1, $r1
	sub45	$r1, $r2
	ret5	$lp
	.size	__negdi2, .-__negdi2

#define  V1H  $r1
#define  V1L  $r0
#define  V2H  $r3
#define  V2L  $r2

	.text
	.align	2
	.globl	__divdi3
	.type	__divdi3, @function
__divdi3:
	push25	$r10, 0
	move	$r8, V1L
	move	$r9, V1H
	move	$r6, V2L
	move	$r7, V2H
	movi	$r10, 0
	bgez	V1H, .L80
	bal	__negdi2
	move	$r8, V1L
	move	$r9, V1H
	movi	$r10, -1
.L80:
	bgez	$r7, .L81
	move	V1L, $r6
	move	V1H, $r7
	bal	__negdi2
	move	$r6, V1L
	move	$r7, V1H
	nor	$r10, $r10, $r10
.L81:
	move	V2L, $r6
	move	V2H, $r7
	move	V1L, $r8
	move	V1H, $r9
	movi	$r4, 0
	bal	__udivmoddi4
	beqz	$r10, .L82
	bal	__negdi2
.L82:
	pop25	$r10, 0
	.size	__divdi3, .-__divdi3
