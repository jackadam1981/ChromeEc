/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Math utility functions for ARMv7 */

#ifndef __EC_MATH_H
#define __EC_MATH_H

static inline float sqrtf(float v)
{
	float root;
	asm volatile(
		"fsqrts %0, %1"
		: "=w" (root)
		: "w" (v)
	);
	return root;
}

#endif  /* __EC_MATH_H */
