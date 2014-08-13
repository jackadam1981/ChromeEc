/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test motion sense code.
 */

#include <math.h>
#include <stdio.h>
#include "math_util.h"
#include "motion_sense.h"
#include "test_util.h"
#include "util.h"

/*****************************************************************************/
/* Need to define motion sensor globals just to compile. */
struct motion_sensor_t motion_sensors[] = {};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

/*****************************************************************************/
/* Test utilities */

/* Macro to compare two floats and check if they are equal within diff. */
#define IS_FLOAT_EQUAL(a, b, diff) ((a) >= ((b) - diff) && (a) <= ((b) + diff))

#define ACOS_TOLERANCE_DEG 0.5f
#define RAD_TO_DEG (180.0f / 3.1415926f)

static int test_vector(vector_3_t *pv, vector_3_t v)
{
	printf("%p = %p\n", pv, v);
	TEST_ASSERT(((void *)pv) == ((void *)v));
	return EC_SUCCESS;
}

static int test_matrix(matrix_3x3_t *pm, matrix_3x3_t m)
{
	printf("%p = %p\n", pm, m);
	TEST_ASSERT(((void *)pm) == ((void *)m));
	return EC_SUCCESS;
}

static int test_func_args(void)
{
	matrix_3x3_t m;
	vector_3_t v;
	return test_vector(&v, v) && test_matrix(&m, m);
}

static int test_acos(void)
{
	float a, b;
	float test;

	/* Test a handful of values. */
	for (test = -1.0; test <= 1.0; test += 0.01) {
		a = arc_cos(test);
		b = acos(test) * RAD_TO_DEG;
		TEST_ASSERT(IS_FLOAT_EQUAL(a, b, ACOS_TOLERANCE_DEG));
	}

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();
	RUN_TEST(test_func_args);
	RUN_TEST(test_acos);

	test_print_result();
}
