/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include "math_util.h"
#include "math.h"
#include "motion_sense.h"
#include "random.h"
#include "test_util.h"

/*****************************************************************************/
/*
 * Need to define motion sensor globals just to compile.
 * We include motion task to force the inclusion of math_util.c
 */
struct motion_sensor_t motion_sensors[] = {};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

/*****************************************************************************/

static float normal(float x, float mean, float stddev)
{
	return powf(M_E, -(x - mean) * (x - mean) / (2 * stddev * stddev)) /
			sqrtf(2 * M_PI * stddev * stddev);
}

static int test_uniform_same_data_for_same_seed(void)
{
	int seed;
	float values[2];

	seed = 4321;
	values[0] = rnd_uniform(&seed);
	TEST_NE(4321, seed, "%d");

	seed = 4321;
	values[1] = rnd_uniform(&seed);
	TEST_NEARF(values[0], values[1], 1e-6f);

	return EC_SUCCESS;
}

static int test_normal_01_same_data_for_same_seed(void)
{
	int seed;
	float values[2];

	seed = 4321;
	values[0] = rnd_normal_01(&seed);
	TEST_NE(4321, seed, "%d");

	seed = 4321;
	values[1] = rnd_normal_01(&seed);
	TEST_NEARF(values[0], values[1], 1e-6f);

	return EC_SUCCESS;
}

static int test_normal_same_data_for_same_seed(void)
{
	int seed;
	float values[2];

	seed = 4321;
	values[0] = rnd_normal(1.0f, 2.5f, &seed);
	TEST_NE(4321, seed, "%d");

	seed = 4321;
	values[1] = rnd_normal(1.0f, 2.5f, &seed);
	TEST_NEARF(values[0], values[1], 1e-6f);

	return EC_SUCCESS;
}

static int test_uniform_seed_changed(void)
{
	int seed = 30;

	rnd_uniform(&seed);
	TEST_NE(30, seed, "%d");
	return EC_SUCCESS;
}

static int test_normal_01_seed_changed(void)
{
	int seed = 30;

	rnd_normal_01(&seed);
	TEST_NE(30, seed, "%d");
	return EC_SUCCESS;
}

static int test_normal_seed_changed(void)
{
	int seed = 30;

	rnd_normal(1.0f, 2.5f, &seed);
	TEST_NE(30, seed, "%d");
	return EC_SUCCESS;
}

static int test_uniform_generate_based_on_seed(void)
{
	int seed1 = 4321, seed2 = 8765, num_equals = 0, i;

	for (i = 0; i < 500; ++i) {
		float val1 = rnd_uniform(&seed1);
		float val2 = rnd_uniform(&seed2);
		float delta = ABS(val1 - val2);

		if (delta < 1e-6f)
			++num_equals;
	}
	TEST_LT(num_equals, 5, "%d");

	return EC_SUCCESS;
}

static int test_normal_01_generate_based_on_seed(void)
{
	int seed1 = 4321, seed2 = 8765, num_equals = 0, i;

	for (i = 0; i < 500; ++i) {
		float val1 = rnd_normal_01(&seed1);
		float val2 = rnd_normal_01(&seed2);
		float delta = ABS(val1 - val2);

		if (delta < 1e-6f)
			++num_equals;
	}
	TEST_LT(num_equals, 5, "%d");

	return EC_SUCCESS;
}

static int test_normal_generate_based_on_seed(void)
{
	int seed1 = 4321, seed2 = 8765, num_equals = 0, i;

	for (i = 0; i < 500; ++i) {
		float val1 = rnd_normal(1.0f, 2.5f, &seed1);
		float val2 = rnd_normal(1.0f, 2.5f, &seed2);
		float delta = ABS(val1 - val2);

		if (delta < 1e-6f)
			++num_equals;
	}
	TEST_LT(num_equals, 5, "%d");

	return EC_SUCCESS;
}

static int test_uniform_range(void)
{
	int seed = 75842, i;
	float min = rnd_uniform(&seed), max = min;

	for (i = 0; i < 10000; ++i) {
		float val = rnd_uniform(&seed);

		min = (val < min) ? val : min;
		max = (val > min) ? val : max;
	}
	/* Test that min >= 0 */
	TEST_GE(min, 0.0f, "%f");
	/* Test that max <= 1 */
	TEST_LE(max, 1.0f, "%f");

	return EC_SUCCESS;
}

/**
 * Tests for uniform distribution by allocating N bins then getting 5*N samples.
 * The samples will be considered uniformly distributed if at least 95% of the
 * bins have a sample in them.
 */
static int test_uniform_distribution(void)
{
	int seed = 37252, count = 0, i;
	uint8_t values[1000];

	memset(values, 0, sizeof(values));
	for (i = 0; i < 5000; ++i) {
		int val = (int) (rnd_uniform(&seed) * 1000);

		/* If rnd_uniform returns exactly 1 we'll overflow */
		val = (val == 1000) ? 999 : val;
		values[val] = 1;
	}
	for (i = 0; i < 1000; ++i) {
		if (values[i])
			++count;
	}
	TEST_GE(count, 990, "%d");

	return EC_SUCCESS;
}

static int test_normal_01_mean(void)
{
	const int num_iterations = 1000;
	int seed = 993, i;
	float sum = 0.0f;

	for (i = 0; i < num_iterations; ++i)
		sum += rnd_normal_01(&seed);

	sum /= num_iterations;
	TEST_NEARF(sum, 0.0f, 0.01f);

	return EC_SUCCESS;
}

static int test_normal_mean(void)
{
	const int num_iterations = 1000;
	int seed = 993, i;
	float sum = 0.0f;

	for (i = 0; i < num_iterations; ++i)
		sum += rnd_normal(1.0f, 2.5f, &seed);

	sum /= num_iterations;
	TEST_NEARF(sum, 1.0f, 0.025);

	return EC_SUCCESS;
}

/**
 * Tests for normal distribution by filling 1,000 bins with 10,000 samples.
 * Samples that are more than 4x the standard deviation are discarded.
 */
static int test_normal_01_distribution(void)
{
	const int num_cols = 100;
	const int num_pts = 10000;
	const float chi_inv = 98.33413733985f; /* CHIINV(0.5, num_cols - 1) */

	int seed = 30045, discard_count = 0, i;
	int bins[num_cols];
	float error_sum = 0.0f;

	i = 0;
	memset(bins, 0, sizeof(bins));
	while (i < num_pts) {
		int idx;
		float val = rnd_normal_01(&seed);

		if (val < -4.0f || val > 4.0f) {
			++discard_count;
			TEST_LT(discard_count, ((int) num_pts * 0.0005), "%d");
			continue;
		}
		idx = (int) ((val + 4.0f) * num_cols / 8.0f);
		idx = (idx == num_cols) ? (num_cols - 1) : idx;
		bins[idx]++;
		++i;
	}

	for (i = 0; i < num_cols; ++i) {
		float x[2], expected, error;

		x[0] = (i * 8.0f / num_cols) - 4.0f;
		x[1] = ((i + 1) * 8.0f / num_cols) - 4.0f;
		expected = normal((x[0] + x[1]) / 2.0f, 0.0f, 1.0f);
		expected *= 8.0f / num_cols;
		expected *= num_pts;
		expected = roundf(expected);
		error = expected - bins[i];
		if (expected > 0)
			error_sum += error * error / expected;
	}

	TEST_LT(error_sum, chi_inv, "%f");

	return EC_SUCCESS;
}

static int test_normal_distribution(void)
{
	const int num_cols = 100;
	const int num_pts = 10000;
	const float chi_inv = 98.33413733985f; /* CHIINV(0.5, num_cols - 1) */
	const float mean = 1.0f;
	const float std_dev = 2.5f;
	const float range = 8 * std_dev;
	const float range_min = mean - (range / 2.0f);
	const float range_max = mean + (range / 2.0f);

	int seed = 30045, discard_count = 0, i;
	int bins[num_cols];
	float error_sum = 0.0f;

	i = 0;
	memset(bins, 0, sizeof(bins));
	while (i < num_pts) {
		int idx;
		float val = rnd_normal(mean, std_dev, &seed);

		if (val < range_min || val > range_max) {
			++discard_count;
			TEST_LT(discard_count, ((int) num_pts * 0.0005), "%d");
			continue;
		}
		idx = (int) ((val - mean + range / 2.0f) * num_cols / range);
		idx = (idx == num_cols) ? (num_cols - 1) : idx;
		bins[idx]++;
		++i;
	}

	for (i = 0; i < num_cols; ++i) {
		float x[2], expected, error;

		x[0] = (i * range / num_cols) - range / 2.0f;
		x[1] = ((i + 1) * range / num_cols) - range / 2.0f;
		expected = normal((x[0] + x[1]) / 2.0f, 0.0f, std_dev);
		expected *= range / num_cols;
		expected *= num_pts;
		expected = roundf(expected);
		error = expected - bins[i];
		if (expected > 0)
			error_sum += error * error / expected;
	}

	TEST_LT(error_sum, chi_inv, "%f");

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_uniform_same_data_for_same_seed);
	RUN_TEST(test_uniform_seed_changed);
	RUN_TEST(test_uniform_generate_based_on_seed);
	RUN_TEST(test_uniform_range);
	RUN_TEST(test_uniform_distribution);

	RUN_TEST(test_normal_01_same_data_for_same_seed);
	RUN_TEST(test_normal_01_seed_changed);
	RUN_TEST(test_normal_01_generate_based_on_seed);
	RUN_TEST(test_normal_01_mean);
	RUN_TEST(test_normal_01_distribution);

	RUN_TEST(test_normal_same_data_for_same_seed);
	RUN_TEST(test_normal_seed_changed);
	RUN_TEST(test_normal_generate_based_on_seed);
	RUN_TEST(test_normal_mean);
	RUN_TEST(test_normal_distribution);

	test_print_result();
}
