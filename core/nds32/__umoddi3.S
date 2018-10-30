/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * __umoddi3.S: unsigned 64-bit remainder
 */

#define P1H	$r1
#define P1L	$r0
#define OFFSET_H 4
#define OFFSET_L 0

	.text
	.align	2
	.globl	__umoddi3
	.type	__umoddi3, @function
__umoddi3:
	! =====================================================================
	! stack allocation:
	! sp+12 +-----------------------+
	!       | remainder             |
	! sp+4  +-----------------------+
	!       | $lp                   |
	! sp    +-----------------------+
	! =====================================================================
	addi	$sp, $sp, -12
	swi	$lp, [$sp+(0)]

	addi	$r4, $sp, 4
	bal	__udivmoddi4
	lwi	P1L, [$sp+(4+OFFSET_L)]
	lwi	P1H, [$sp+(4+OFFSET_H)]

	! epilogue
	lwi.bi $lp, [$sp], 12
	ret
	.size	__umoddi3, .-__umoddi3
