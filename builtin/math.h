/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_MATH_H__
#define __CROS_EC_MATH_H__

#include <stdbool.h>
#include "fpu.h"

static inline bool isnan(float a)
{
	uint32_t x = *(uint32_t *)&a;

	/* Exponent must have all bits set and fraction must be non-zero */
	return ((x & 0x7f800000) == 0x7f800000) && ((x & 0x7fffff) != 0);
}

static inline bool isinf(float a)
{
	uint32_t x = *(uint32_t *)&a;

	/* Exponent must have all bits set and fraction must be zero */
	return (x & 0x7fffffff) == 0x7f800000;
}

#endif /* __CROS_EC_MATH_H__ */
