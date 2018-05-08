/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Math utility functions for minute-IA */

#ifndef __CROS_EC_MATH_H
#define __CROS_EC_MATH_H

static inline float sqrtf(float v)
{
	float root;

	/* root = fsqart (v); */
	asm volatile(
		"fsqrt"
		: "=t" (root)
		: "0" (v)
	);
	return root;
}

static inline float fabsf(float v)
{
	float root;

	/* root = fabs (v); */
	asm volatile(
		"fabs"
		: "=t" (root)
		: "0" (v)
	);
	return root;
}

#endif  /* __CROS_EC_MATH_H */
