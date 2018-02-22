/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Enable the use of right shift for uint64_t. */

#include <stdint.h>

union words {
	uint64_t u64;
	uint32_t w[2];
};

uint64_t __aeabi_llsr(uint64_t v, uint32_t shift)
{
	union words val;
	union words res;

	val.u64 = v;
	res.w[1] = val.w[1] >> shift;
	res.w[0] = val.w[0] >> shift;
	res.w[0] |= val.w[1] >> (shift - 32); /* Handle shift >= 32*/
	res.w[0] |= val.w[1] << (32 - shift); /* Handle shift <= 32*/
	return res.u64;
}
