/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <math.h>

/*
 * Explicily include common.h to populate predefined macros in test_config.h
 * early. e.g. CONFIG_FPU, which is needed in math_util.h
 */
#include "common.h"

#include "math_util.h"
#include "test_util.h"
#include "vec3.h"

#if defined(TEST_FP) && !defined(CONFIG_FPU)
#define NORM_TOLERANCE FLOAT_TO_FP(0.01f)
#define NORM_SQUARED_TOLERANCE FLOAT_TO_FP(0.0f)
#define DOT_TOLERANCE FLOAT_TO_FP(0.001f)
#elif defined(TEST_FLOAT) && defined(CONFIG_FPU)
#define NORM_TOLERANCE FLOAT_TO_FP(0.0f)
#define NORM_SQUARED_TOLERANCE FLOAT_TO_FP(0.0f)
#define DOT_TOLERANCE FLOAT_TO_FP(0.0f)
#else
#error "No such test configuration."
#endif

#define IS_FPV3_VECTOR_EQUAL(a, b, diff)                                       \
	(IS_FP_EQUAL((a)[0], (b)[0], (diff)) &&                                \
	 IS_FP_EQUAL((a)[1], (b)[1], (diff)) &&                                \
	 IS_FP_EQUAL((a)[2], (b)[2], (diff)))
#define IS_FP_EQUAL(a, b, diff) ((a) >= ((b)-diff) && (a) <= ((b) + diff))
#define IS_FLOAT_EQUAL(a, b, diff) IS_FP_EQUAL(a, b, diff)

static int test_fpv3_scalar_mul(void)
{
	const int N = 3;
	const float s = 2.0f;
	int i;
	floatv3_t r = {1.0f, 2.0f, 4.0f};
	fpv3_t a, fpr;

	for (i = 0; i < N; ++i)
		a[i] = FLOAT_TO_FP(r[i]);

	for (i = 0; i < N; ++i)
		r[i] *= s;

	for (i = 0; i < N; ++i)
		fpr[i] = FLOAT_TO_FP(r[i]);

	fpv3_scalar_mul(a, FLOAT_TO_FP(s));

	TEST_ASSERT(IS_FPV3_VECTOR_EQUAL(a, fpr, FLOAT_TO_FP(0.0f)));

	return EC_SUCCESS;
}

static int test_fpv3_dot(void)
{
	const int N = 3;
	int i;
	float r = 0.0f;
	floatv3_t a = {1.8f, 2.12f, 4.12f};
	floatv3_t b = {3.1f, 4.3f, 5.8f};
	fpv3_t fpa, fpb;

	for (i = 0; i < N; ++i) {
		fpa[i] = FLOAT_TO_FP(a[i]);
		fpb[i] = FLOAT_TO_FP(b[i]);
	}

	for (i = 0; i < N; ++i)
		r += a[i] * b[i];

	TEST_ASSERT(IS_FP_EQUAL(fpv3_dot(fpa, fpb), FLOAT_TO_FP(r),
				DOT_TOLERANCE));

	return EC_SUCCESS;
}

static int test_fpv3_norm_squared(void)
{
	const int N = 3;
	int i;
	float r = 0.0f;
	floatv3_t a = {3.0f, 4.0f, 5.0f};
	fpv3_t fpa;

	for (i = 0; i < N; ++i)
		fpa[i] = FLOAT_TO_FP(a[i]);

	for (i = 0; i < N; ++i)
		r += a[i] * a[i];

	TEST_ASSERT(IS_FP_EQUAL(fpv3_norm_squared(fpa), FLOAT_TO_FP(r),
				NORM_SQUARED_TOLERANCE));

	return EC_SUCCESS;
}

static int test_fpv3_norm(void)
{
	const int N = 3;
	int i;
	float r = 0.0f;
	floatv3_t a = {3.1f, 4.2f, 5.3f};
	fpv3_t fpa;

	for (i = 0; i < N; ++i)
		fpa[i] = FLOAT_TO_FP(a[i]);

	for (i = 0; i < N; ++i)
		r += a[i] * a[i];

	r = sqrtf(r);

	TEST_ASSERT(
		IS_FP_EQUAL(fpv3_norm(fpa), FLOAT_TO_FP(r), NORM_TOLERANCE));

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_fpv3_scalar_mul);
	RUN_TEST(test_fpv3_dot);
	RUN_TEST(test_fpv3_norm_squared);
	RUN_TEST(test_fpv3_norm);

	test_print_result();
}
