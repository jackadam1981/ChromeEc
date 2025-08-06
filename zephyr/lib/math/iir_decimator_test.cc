/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros/math/iir_decimator.h"

#include <gtest/gtest.h>

namespace
{

TEST(IirDecimator, Init)
{
	IIR_DECIMATOR(decimator, 100, 2);

	// Expecting 100mHz, got 1000mHz: 1000 / 100 = 10
	EXPECT_EQ(iir_decimator_init(&decimator, 1000), 0);
	EXPECT_EQ(decimator.decimation_factor, 10u);
}

TEST(IirDecimator, InitRounding)
{
	IIR_DECIMATOR(decimator, 100, 2);

	// 1049 mHz -> 1049/100 = 10.49 -> 10
	EXPECT_EQ(iir_decimator_init(&decimator, 1049), 0);
	EXPECT_EQ(decimator.decimation_factor, 10u);

	// 1050 mHz -> 1050/100 = 10.5 -> 11
	EXPECT_EQ(iir_decimator_init(&decimator, 1050), 0);
	EXPECT_EQ(decimator.decimation_factor, 11u);
}

TEST(IirDecimator, InitZeroSampleRate)
{
	IIR_DECIMATOR(decimator, 100, 2);

	EXPECT_EQ(iir_decimator_init(&decimator, 0), 0);
	EXPECT_EQ(decimator.decimation_factor, 0u);
}

TEST(IirDecimator, InitDecimationFactorZero)
{
	IIR_DECIMATOR(decimator, 2000, 2);

	// 1000 mHz -> 1000/2000 = 0.5 -> 0 -> 1
	EXPECT_EQ(iir_decimator_init(&decimator, 500), 0);
	EXPECT_EQ(decimator.decimation_factor, 1u);
}

TEST(IirDecimator, InitFilterInitFails)
{
	IIR_DECIMATOR(decimator, 100, 20);

	EXPECT_NE(iir_decimator_init(&decimator, 1000), 0);
}

TEST(IirDecimator, Step)
{
	IIR_DECIMATOR(decimator, 100, 2);
	float x = 1.0f, y = 2.0f, z = 3.0f;

	ASSERT_EQ(iir_decimator_init(&decimator, 1000), 0);

	for (int i = 0; i < 9; ++i) {
		EXPECT_FALSE(iir_decimator_step(&decimator, &x, &y, &z));
	}
	EXPECT_TRUE(iir_decimator_step(&decimator, &x, &y, &z));
	EXPECT_NE(x, 1.0f);
	EXPECT_NE(y, 2.0f);
	EXPECT_NE(z, 3.0f);
}

TEST(IirDecimator, StepNoDecimation)
{
	IIR_DECIMATOR(decimator, 100, 2);
	float x = 1.0f, y = 2.0f, z = 3.0f;

	ASSERT_EQ(iir_decimator_init(&decimator, 100), 0);
	EXPECT_EQ(decimator.decimation_factor, 1u);

	EXPECT_TRUE(iir_decimator_step(&decimator, &x, &y, &z));
}

TEST(IirDecimator, StepDecimationFactorZero)
{
	IIR_DECIMATOR(decimator, 100, 2);
	float x = 1.0f, y = 2.0f, z = 3.0f;

	decimator.decimation_factor = 0;

	EXPECT_TRUE(iir_decimator_step(&decimator, &x, &y, &z));
}

} // namespace
