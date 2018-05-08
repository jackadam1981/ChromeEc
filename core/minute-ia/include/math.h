/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Math utility functions for minute-IA */

#ifndef __CROS_EC_MATH_H
#define __CROS_EC_MATH_H

#define M_PI            3.14159265358979323846
#define M_PI_2          1.57079632679489661923

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

static inline float logf(float v)
{
	float res;

	asm volatile(
		"fldln2\n"
		"fxch\n"
		"fyl2x\n"
		: "=t" (res)
		: "0" (v));

	return res;
}

static inline float log(float v)
{
	return logf(v);
}

static inline float expf(float v)
{
	float res;

	asm volatile(
		"fldl2e\n"
		"fmul %%st(1)\n"
		"fst %%st(1)\n"
		"frndint\n"
		"fxch\n"
		"fsub %%st(1)\n"
		"f2xm1\n"
		"fld1\n"
		"faddp\n"
		"fscale\n"
		: "=t" (res)
		: "0" (v));
	return res;
}

static inline float powf(float x, float y)
{
	float res, exponent;

	asm volatile(
		"fyl2x"
		: "=t" (res)
		: "0" (x), "u" (1.0));
	asm volatile(
		"fmul %%st(1)\n"
		"fst %%st(1)\n"
		"frndint\n"
		"fxch\n"
		"fsub %%st(1)\n"
		"f2xm1\n"
		: "=t" (res), "=u" (exponent)
		: "0" (y), "1" (res));
	res += 1.0;
	asm volatile(
		"fscale"
		: "=t" (res)
		: "0" (res), "u" (exponent));

	return res;
}

static inline float ceilf(float v)
{
	float res;
	unsigned short control_word, control_word_tmp;

	asm volatile("fnstcw %0" : "=m" (control_word));
	control_word_tmp = (control_word | 0x0800) & 0xfbff;
	asm volatile(
		"fldcw %1\n"
		"frndint\n"
		"fldcw %2"
		: "=t" (res)
		: "m" (control_word_tmp), "m"(control_word));

	return res;
}

static inline float atan2f(float x, float y)
{
	float res;

	asm volatile("fpatan" : "=t" (res) : "0" (x), "u" (y) : "st(1)");

	return res;
}

static inline float atanf(float v)
{
	float res;

	asm volatile(
		"fld1\n"
		"fpatan\n"
		: "=t" (res)
		: "0" (v));

	return res;
}

static inline float sinf(float v)
{
	float res;

	asm volatile("fsin" : "=t" (res) : "0" (v));

	return res;
}

static inline float cosf(float v)
{
	float res;

	asm volatile("fcos" : "=t" (res) : "0" (v));

	return res;
}

static inline float acosf(float v)
{
	return atan2f(sqrtf(1.0 - v * v), v);
}

static inline float __isnanf(float v)
{
	unsigned int x = *((unsigned int *)&v);

	x = 0x7f800000 - (x & 0x7fffffff);

	return (int)(x >> 31);
}

static inline float __isinff(float v)
{
	unsigned int x = *((unsigned int *)&v);
	unsigned int t;

	t = 0x7f800000 - (x & 0x7fffffff);
	t |= -t;

	return ~(t >> 31) & (x >> 30);
}

#endif  /* __CROS_EC_MATH_H */
