/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "stillness_detector.h"
#include "motion_sense.h"
#include "test_util.h"
#include <stdio.h>

#define MS(t) (t * 1000)

/*****************************************************************************/
/*
 * Need to define motion sensor globals just to compile.
 * We include motion task to force the inclusion of math_util.c
 */
struct motion_sensor_t motion_sensors[] = {};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

static int test_not_still_short_window(void)
{
	struct still_det det;
	int i;

	stillDetInit(&det, 0.00025f, MS(800), MS(1200), 5);
	for (i = 0; i < 6; ++i)
		TEST_ASSERT(!stillDetUpdate(&det, i * MS(100),
					    0.0f, 0.0f, 0.0f));

	return EC_SUCCESS;
}

static int test_not_still_long_window(void)
{
	struct still_det det;
	int i;

	stillDetInit(&det, 0.00025f, MS(800), MS(1200), 5);
	for (i = 0; i < 5; ++i)
		TEST_ASSERT(!stillDetUpdate(&det, i * MS(300),
					    0.0f, 0.0f, 0.0f));

	return EC_SUCCESS;
}

static int test_not_still_not_enough_samples(void)
{
	struct still_det det;
	int i;

	stillDetInit(&det, 0.00025f, MS(800), MS(1200), 5);
	for (i = 0; i < 4; ++i)
		TEST_ASSERT(!stillDetUpdate(&det, i * MS(200),
					    0.0f, 0.0f, 0.0f));

	return EC_SUCCESS;
}

static int test_is_still_all_axes(void)
{
	struct still_det det;
	int i;

	stillDetInit(&det, 0.00025f, MS(800), MS(1200), 5);
	for (i = 0; i < 9; ++i) {
		int result = stillDetUpdate(&det, i * MS(100),
					    i * 0.001f, i * 0.001f, i * 0.001f);

		TEST_EQ(result, i == 8 ? 1 : 0, "%d");
	}
	TEST_NEARF(det.mean_x, 0.004f, 0.0001f);
	TEST_NEARF(det.mean_y, 0.004f, 0.0001f);
	TEST_NEARF(det.mean_z, 0.004f, 0.0001f);

	return EC_SUCCESS;
}

static int test_not_still_one_axis(void)
{
	struct still_det det;
	int i;

	stillDetInit(&det, 0.00025f, MS(800), MS(1200), 5);
	for (i = 0; i < 9; ++i) {
		TEST_ASSERT(!stillDetUpdate(&det, i * MS(100),
					    i * 0.001f, i * 0.001f, i * 0.01f));
	}

	return EC_SUCCESS;
}

static int test_resets(void)
{
	struct still_det det;
	int i;

	stillDetInit(&det, 0.00025f, MS(800), MS(1200), 5);
	for (i = 0; i < 9; ++i) {
		TEST_ASSERT(!stillDetUpdate(&det, i * MS(100),
					    i * 0.001f, i * 0.001f, i * 0.01f));
	}

	for (i = 0; i < 9; ++i) {
		int result = stillDetUpdate(&det, i * MS(100),
					    i * 0.001f, i * 0.001f, i * 0.001f);

		TEST_EQ(result, i == 8 ? 1 : 0, "%d");
	}
	TEST_NEARF(det.mean_x, 0.004f, 0.0001f);
	TEST_NEARF(det.mean_y, 0.004f, 0.0001f);
	TEST_NEARF(det.mean_z, 0.004f, 0.0001f);

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_not_still_short_window);
	RUN_TEST(test_not_still_long_window);
	RUN_TEST(test_not_still_not_enough_samples);
	RUN_TEST(test_is_still_all_axes);
	RUN_TEST(test_not_still_one_axis);
	RUN_TEST(test_resets);

	test_print_result();
}
