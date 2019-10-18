/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "newton_fit.h"
#include "motion_sense.h"
#include "test_util.h"
#include <stdio.h>

/*
 * Need to define motion sensor globals just to compile.
 * We include motion task to force the inclusion of math_util.c
 */
struct motion_sensor_t motion_sensors[] = {};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

static int test_newton_fit_reset(void)
{
	struct newton_fit fit = NEWTON_FIT(4, 0.01f, 0.25f, 1.0e-8f, 100);

	newton_fit_reset(&fit);
	newton_fit_accumulate(&fit, 1.0f, 0.0f, 0.0f);
	newton_fit_reset(&fit);
	TEST_EQ(fit.state->num_orientations, 0, "%u");

	return EC_SUCCESS;
}

static int test_newton_fit_accumulate(void)
{
	struct newton_fit fit = NEWTON_FIT(4, 0.01f, 0.25f, 1.0e-8f, 100);
	int res;

	newton_fit_reset(&fit);

	res = newton_fit_accumulate(&fit, 1.0f, 0.0f, 0.0f);
	TEST_EQ(res, 1, "%d");
	TEST_EQ(fit.state->num_orientations, 1, "%u");

	return EC_SUCCESS;
}

static int test_newton_fit_accumulate_merge(void)
{
	struct newton_fit fit = NEWTON_FIT(4, 0.01f, 0.25f, 1.0e-8f, 100);
	int res;

	newton_fit_reset(&fit);

	res = newton_fit_accumulate(&fit, 1.0f, 0.0f, 0.0f);
	TEST_EQ(res, 1, "%d");
	res = newton_fit_accumulate(&fit, 1.05f, 0.0f, 0.0f);
	TEST_EQ(res, 1, "%d");
	TEST_EQ(fit.state->num_orientations, 1, "%u");

	return EC_SUCCESS;
}

static int test_newton_fit_accumulate_reject(void)
{
	struct newton_fit fit = NEWTON_FIT(4, 0.01f, 0.25f, 1.0e-8f, 100);
	int res;

	newton_fit_reset(&fit);

	res = newton_fit_accumulate(&fit, 1.0f, 0.0f, 0.0f);
	res += newton_fit_accumulate(&fit, -1.0f, 0.0f, 0.0f);
	res += newton_fit_accumulate(&fit, 0.0f, 1.0f, 0.0f);
	res += newton_fit_accumulate(&fit, 0.0f, -1.0f, 0.0f);
	TEST_EQ(res, 4, "%d");
	TEST_EQ(fit.state->num_orientations, 4, "%u");

	res = newton_fit_accumulate(&fit, 0.0f, 0.0f, 1.0f);
	TEST_EQ(res, 0, "%d");

	return EC_SUCCESS;
}

static int test_newton_fit_calculate(void)
{
	struct newton_fit fit = NEWTON_FIT(4, 0.01f, 0.25f, 1.0e-8f, 100);
	floatv3_t bias;
	float radius;

	newton_fit_reset(&fit);

	newton_fit_accumulate(&fit, 1.01f, 0.01f, 0.01f);
	newton_fit_accumulate(&fit, -0.99f, 0.01f, 0.01f);
	newton_fit_accumulate(&fit, 0.01f, 1.01f, 0.01f);
	newton_fit_accumulate(&fit, 0.01f, 0.01f, 1.01f);

	fpv3_init(bias, 0.0f, 0.0f, 0.0f);
	newton_fit_compute(&fit, bias, &radius);

	TEST_NEAR(bias[0], 0.01f, 0.0001f, "%f");
	TEST_NEAR(bias[1], 0.01f, 0.0001f, "%f");
	TEST_NEAR(bias[2], 0.01f, 0.0001f, "%f");
	TEST_NEAR(radius, 1.0f, 0.0001f, "%f");

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_newton_fit_reset);
	RUN_TEST(test_newton_fit_accumulate);
	RUN_TEST(test_newton_fit_accumulate_merge);
	RUN_TEST(test_newton_fit_accumulate_reject);
	RUN_TEST(test_newton_fit_calculate);

	test_print_result();
}
