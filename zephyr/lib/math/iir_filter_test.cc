/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros/math/iir_filter.h"

#include <cmath>
#include <gtest/gtest.h>

namespace
{

// Note: The public header is `cros/math/iir_filter.h` but the implementation
// in `zephyr/lib/math/iir_filter.c` uses a different name for the step
// function (`filter_iir_step` instead of `iir_filter_step`). This test is
// written against the public header and will require the implementation to be
// fixed.

TEST(IirFilter, Init)
{
	struct iir_filter_t filter;
	float a[] = { 1.0f, 0.2f, 0.3f };
	float b[] = { 0.4f, 0.5f, 0.6f };
	const uint8_t rank = 2;

	EXPECT_EQ(iir_filter_init(&filter, rank, a, b), 0);
	EXPECT_EQ(filter.rank, rank);

	for (int i = 0; i <= rank; ++i) {
		EXPECT_EQ(filter.params[i].a, a[i]);
		EXPECT_EQ(filter.params[i].b, b[i]);
		EXPECT_EQ(filter.params[i].x, 0.0f);
		EXPECT_EQ(filter.params[i].y, 0.0f);
	}
}

TEST(IirFilter, InitInvalidRank)
{
	struct iir_filter_t filter;
	float a[FILTER_RANK_MAX + 2];
	float b[FILTER_RANK_MAX + 2];
	EXPECT_NE(iir_filter_init(&filter, FILTER_RANK_MAX + 1, a, b), 0);
}

TEST(IirFilter, StepPassthrough)
{
	struct iir_filter_t filter;
	float a[] = { 1.0f };
	float b[] = { 1.0f };
	const uint8_t rank = 0;

	iir_filter_init(&filter, rank, a, b);

	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, -5.0f), -5.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 0.0f), 0.0f);
}

TEST(IirFilter, StepGain)
{
	struct iir_filter_t filter;
	float a[] = { 1.0f };
	float b[] = { 2.5f };
	const uint8_t rank = 0;

	iir_filter_init(&filter, rank, a, b);

	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 1.0f), 2.5f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, -2.0f), -5.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 2.0f), 5.0f);
}

TEST(IirFilter, StepMovingAverage)
{
	struct iir_filter_t filter;
	// y[k] = 0.5*x[k] + 0.5*x[k-1]
	float a[] = { 1.0f, 0.0f };
	float b[] = { 0.5f, 0.5f };
	const uint8_t rank = 1;

	iir_filter_init(&filter, rank, a, b);

	// Step response
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 10.0f), 5.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 10.0f), 10.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 10.0f), 10.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 0.0f), 5.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 0.0f), 0.0f);
}

TEST(IirFilter, StepLeakyIntegrator)
{
	struct iir_filter_t filter;
	// y[k] = x[k] + 0.9*y[k-1]
	float a[] = { 1.0f, -0.9f };
	float b[] = { 1.0f, 0.0f };
	const uint8_t rank = 1;

	iir_filter_init(&filter, rank, a, b);

	// Impulse response
	EXPECT_NEAR(iir_filter_step(&filter, 10.0f), 10.0f, 1e-6f);
	EXPECT_NEAR(iir_filter_step(&filter, 0.0f), 9.0f, 1e-6f);
	EXPECT_NEAR(iir_filter_step(&filter, 0.0f), 8.1f, 1e-6f);
	EXPECT_NEAR(iir_filter_step(&filter, 0.0f), 7.29f, 1e-6f);

	// Step response
	iir_filter_init(&filter, rank, a, b);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 10.0f), 10.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 10.0f), 19.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 10.0f), 27.1f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 10.0f), 34.39f);
}

TEST(IirFilter, StepSecondOrder)
{
	struct iir_filter_t filter;
	// y(k) = x(k) + 1.5*y(k-1) - 0.5*y(k-2)
	float a[] = { 1.0f, -1.5f, 0.5f };
	float b[] = { 1.0f, 0.0f, 0.0f };
	const uint8_t rank = 2;

	iir_filter_init(&filter, rank, a, b);

	// Impulse response
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 0.0f), 1.5f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 0.0f), 1.75f);
	EXPECT_FLOAT_EQ(iir_filter_step(&filter, 0.0f), 1.875f);
}

TEST(IirFilter, ButterworthLpfInit)
{
	struct iir_filter_t filter;
	const uint8_t rank = 2;
	const float freq = 0.1f;

	EXPECT_EQ(filter_butterworth_lpf_init(&filter, rank, freq), 0);
	EXPECT_EQ(filter.rank, rank);
}

TEST(IirFilter, ButterworthLpfInitInvalidRank)
{
	struct iir_filter_t filter;
	const float freq = 0.1f;

	EXPECT_NE(filter_butterworth_lpf_init(&filter, FILTER_RANK_MAX + 1,
					      freq),
		  0);
}

TEST(IirFilter, ButterworthLpfStep)
{
	struct iir_filter_t filter;
	const uint8_t rank = 3;
	const float freq = 0.25f; // Cutoff at 1/4 of Nyquist frequency
	const float step_input = 10.0f;
	float y = 0.0f;

	ASSERT_EQ(filter_butterworth_lpf_init(&filter, rank, freq), 0);

	// Apply a step input. The output should asymptotically approach the
	// input.
	for (int i = 0; i < 100; ++i) {
		y = iir_filter_step(&filter, step_input);
	}

	// After many steps, the output should be very close to the input.
	EXPECT_NEAR(y, step_input, 1e-5f);

	// Verify that the filter is stable and doesn't blow up.
	EXPECT_TRUE(std::isfinite(y));
}

} // namespace
