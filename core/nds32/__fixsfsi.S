/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

	.text
	.align	2
	.global	__fixsfsi
	.type	__fixsfsi, @function
__fixsfsi:
	push    $lp
	slli    $r1, $r0, #8
	move    $r3, #0x80000000
	or      $r1, $r1, $r3
	srli    $r3, $r0, #23
	andi    $r3, $r3, #0xff
	subri   $r2, $r3, #0x9e
	blez    $r2, .LJspec
	sltsi   $r15, $r2, #0x20
	bnezs8  .Li42
	move    $r0, #0
	j       .LJ999
.Li42:
	srl     $r1, $r1, $r2
	sltsi   $r15, $r0, #0
	beqzs8  .Li43
	subri   $r1, $r1, #0
.Li43:
	move    $r0, $r1
.LJ999:
	pop     $lp
	ret5    $lp
.LJspec:
	move    $r3, #0x7f800000
	slt     $r15, $r3, $r0
	beqzs8  .Li44
	move    $r0, #0x80000000
	j       .LJ999
.Li44:
	move    $r0, #0x7fffffff
	j       .LJ999
	.size	__fixsfsi, .-__fixsfsi
