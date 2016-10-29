/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Math utility functions for minute-IA */

#ifndef __CROS_EC_MATH_H
#define __CROS_EC_MATH_H

#ifdef CONFIG_FPU
static inline float sqrtf(float v)
{
#if 1
	float root;

	asm volatile(
		"fsqrt"
		: "=t" (root)
		: "0" (v)
	);
	return root;
#else
	return 1;
#endif
}

static inline float fabsf(float v)
{
#if 1
	float root;

	asm volatile(
		"fabs"
		: "=t" (root)
		: "0" (v)
	);
	return root;
#else
	return 1;
#endif
}
#endif  /* CONFIG_FPU */

#endif  /* __CROS_EC_MATH_H */
