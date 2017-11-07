/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Software emulation for CTZ instruction
 */

/**
 * Count trailing zeros
 *
 * @param x non null integer.
 * @return the number of trailing 0-bits in x,
 * starting at the least significant bit position.
 */
int __ctzsi2(int x)
{
	int r = 0;

	if (!x)
		return 32;
	if (!(x & 0x0000ffffu)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0x000000ffu)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0x0000000fu)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 0x00000003u)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 0x00000001u)) {
		x >>= 1;
		r += 1;
	}
	return r;
}
