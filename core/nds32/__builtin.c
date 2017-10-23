/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

/*
 * __builtin_ctz:
 * Returns the number of trailing 0-bits in x,
 * starting at the least significant bit position.
 */
int __ctzsi2(int x)
{
	int r = 0;

	if (!x)
		return 32;

	if (!(x & 0x0000ffff)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0x000000ff)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0x0000000f)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 0x00000003)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 0x00000001))
		r += 1;

	return r;
}

/*
 * __builtin_ffs:
 * Returns one plus the index of the least significant 1-bit of x,
 * or if x is zero, returns zero.
 */
int __ffssi2(int x)
{
	return (!x) ? 0 : (__builtin_ctz(x) + 1);
}
