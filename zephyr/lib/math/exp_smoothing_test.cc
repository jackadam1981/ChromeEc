/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "public/cros/math/exp_smoothing.h"

#include <gtest/gtest.h>

namespace
{

TEST(ExpSmoothing, Init)
{
	struct exp_smooth_t filter;
	const float initial_value = 100.0f;
	const float smoothing_factor = 0.2f;

	exp_smooth_init(&filter, smoothing_factor, initial_value);

	EXPECT_FLOAT_EQ(filter.a, smoothing_factor);
	EXPECT_FLOAT_EQ(filter.a_complementary, 1.0f - smoothing_factor);
	EXPECT_FLOAT_EQ(filter.x, initial_value);
}

TEST(ExpSmoothing, Step)
{
	struct exp_smooth_t filter;
	const float initial_value = 100.0f;
	const float smoothing_factor = 0.8f;

	exp_smooth_init(&filter, smoothing_factor, initial_value);

	/* First step */
	float new_value = 200.0f;
	/* expected_value = 0.8 * 100.0 + 0.2 * 200.0 = 80 + 40 = 120.0 */
	float smoothed_value = exp_smooth_step(&filter, new_value);

	EXPECT_FLOAT_EQ(smoothed_value, 120.0f);
	EXPECT_FLOAT_EQ(filter.x, 120.0f);

	/* Second step */
	new_value = 50.0f;
	/* expected_value = 0.8 * 120.0 + 0.2 * 50.0 = 96 + 10 = 106.0 */
	smoothed_value = exp_smooth_step(&filter, new_value);

	EXPECT_FLOAT_EQ(smoothed_value, 106.0f);
	EXPECT_FLOAT_EQ(filter.x, 106.0f);
}

TEST(ExpSmoothing, StepWithZeroFactor)
{
	struct exp_smooth_t filter;
	const float initial_value = 100.0f;
	const float smoothing_factor = 0.0f;

	exp_smooth_init(&filter, smoothing_factor, initial_value);

	/* With a=0, the output should always be the new input value. */
	float new_value = 200.0f;
	float smoothed_value = exp_smooth_step(&filter, new_value);

	EXPECT_FLOAT_EQ(smoothed_value, new_value);
	EXPECT_FLOAT_EQ(filter.x, new_value);

	new_value = -50.0f;
	smoothed_value = exp_smooth_step(&filter, new_value);

	EXPECT_FLOAT_EQ(smoothed_value, new_value);
	EXPECT_FLOAT_EQ(filter.x, new_value);
}

TEST(ExpSmoothing, StepWithOneFactor)
{
	struct exp_smooth_t filter;
	const float initial_value = 100.0f;
	const float smoothing_factor = 1.0f;

	exp_smooth_init(&filter, smoothing_factor, initial_value);

	/* With a=1, the output should always be the initial value. */
	float new_value = 200.0f;
	float smoothed_value = exp_smooth_step(&filter, new_value);

	EXPECT_FLOAT_EQ(smoothed_value, initial_value);
	EXPECT_FLOAT_EQ(filter.x, initial_value);

	new_value = -50.0f;
	smoothed_value = exp_smooth_step(&filter, new_value);

	EXPECT_FLOAT_EQ(smoothed_value, initial_value);
	EXPECT_FLOAT_EQ(filter.x, initial_value);
}

} // namespace
