/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Math utility functions for minute-IA */

#ifndef __CROS_EC_MATH_H
#define __CROS_EC_MATH_H

#ifdef CONFIG_FPU

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

/* Absolute value of V. */
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

/**
 * Natural logarithm of V.
 *
 * @return ln(v)
 */
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

/**
 * Exponential function of V.
 *
 * @return e**v
 */
static inline float expf(float v)
{
	float res;

	asm volatile(
		"fldl2e\n"
		"fmulp\n"
		"fld %%st(0)\n"
		"frndint\n"
		"fsubr %%st(0),%%st(1)\n"
		"fxch %%st(1)\n"
		"f2xm1\n"
		"fld1\n"
		"faddp\n"
		"fscale\n"
		"fstp %%st(1)\n"
		: "=t" (res)
		: "0" (v));

	return res;
}

/**
 * X to the Y power.
 * 
 * @return x**y
 */
static inline float powf(float x, float y)
{
	float res;

	asm volatile(
		"fyl2x\n"
		"fld %%st\n"
		"frndint\n"
		"fsub %%st,%%st(1)\n"
		"fxch\n"
		"fchs\n"
		"f2xm1\n"
		"fld1\n"
		"faddp\n"
		"fxch\n"
		"fld1\n"
		"fscale\n"
		"fstp %%st(1)\n"
		"fmulp\n"
		: "=t" (res)
		: "0" (x), "u" (y)
		: "st(1)");

	return res;
}

/* Smallest integral value not less than V.  */
static inline float ceilf(float v)
{
	float res;
	unsigned short control_word, control_word_tmp;

	asm volatile("fnstcw %0" : "=m" (control_word));
	control_word_tmp = (control_word | 0x0800) & 0xfbff;
	asm volatile(
		"fld %3\n"
		"fldcw %1\n"
		"frndint\n"
		"fldcw %2"
		: "=t" (res)
		: "m" (control_word_tmp), "m"(control_word), "m" (v));

	return res;
}

/* Arc tangent of Y/X.  */
static inline float atan2f(float y, float x)
{
	float res;

	asm volatile("fpatan" : "=t" (res) : "0" (x), "u" (y) : "st(1)");

	return res;
}

/* Arc tangent of V. */
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

/* Sine of V. */
static inline float sinf(float v)
{
	float res;

	asm volatile("fsin" : "=t" (res) : "0" (v));

	return res;
}

/* Cosine of V. */
static inline float cosf(float v)
{
	float res;

	asm volatile("fcos" : "=t" (res) : "0" (v));

	return res;
}

/* Arc cosine of V. */
static inline float acosf(float v)
{
	return atan2f(sqrtf(1.0 - v * v), v);
}

#define CONDITION_MASK_C0   0x00000100
#define CONDITION_MASK_C1   0x00000200
#define CONDITION_MASK_C2   0x00000400
#define CONDITION_MASK_C3   0x00004000

#define X87_STATUS_BUSY (1 << 15)
#define X87_STATUS_

/* Check if V is NaN (not-a-number).  */
static inline int __isnanf(float v)
{
	unsigned int stat;

	asm volatile(
		"fld %1\n"
		"fxam\n"
		"fnstsw %0\n"
		"faddp\n"
		: "=m" (stat)
		: "m" (v));

	return (stat & CONDITION_MASK_C0) &&
	       !(stat & CONDITION_MASK_C2) &&
	       !(stat & CONDITION_MASK_C3);
}

/**
 * Check if V is infinite.
 *
 * @return 0 if V is finite or NaN.
 * @return +1 if V is +infinite.
 * @return -1 if V is -infinite.
 */
static inline int __isinff(float v)
{
	unsigned int stat;

	asm volatile(
		"fld %1\n"
		"fxam\n"
		"fnstsw %0\n"
		"faddp\n"
		: "=m" (stat)
		: "m" (v));

	if ((stat & CONDITION_MASK_C0) && (stat & CONDITION_MASK_C2) &&
	    !(stat & CONDITION_MASK_C3)) {
		/* Infinite number, check sign */
		return stat & CONDITION_MASK_C1 ? -1 : 1;
	}

	/* Finite or NaN */
	return 0;
}

#endif  /* CONFIG_FPU */
#endif  /* __CROS_EC_MATH_H */
