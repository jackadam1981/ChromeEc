/* Copyright 2023 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "bmi160.h"
#include "emul_bmi160.h"
#include "math_util.h"
#include "test/motion_sense/fixtures.h"

#include <zephyr/drivers/sensor.h>

#include <gtest/gtest.h>

namespace
{
constexpr int kRoundDown = 0;
constexpr double kGravity = 9.80665;
constexpr double kPi = 3.141592654;
} /* namespace */

TEST_F(Bmi160, Accel_Read)
{
	constexpr const int8_t kExpectedValueShift = 5;
	constexpr const q31_t kExpectedValues[] = {
		static_cast<q31_t>(-1.75 * kGravity *
				   BIT(31 - kExpectedValueShift)),
		static_cast<q31_t>(-0.11 * kGravity *
				   BIT(31 - kExpectedValueShift)),
		static_cast<q31_t>(1.88 * kGravity *
				   BIT(31 - kExpectedValueShift)),
	};
	intv3_t values_mg;

	ASSERT_EQ(EC_SUCCESS,
		  ms_accel->drv->set_range(ms_accel, 2, kRoundDown));
	ASSERT_EQ(0, emul_sensor_backend_set_channel(emul, SENSOR_CHAN_ACCEL_X,
						     &kExpectedValues[0],
						     kExpectedValueShift));
	ASSERT_EQ(0, emul_sensor_backend_set_channel(emul, SENSOR_CHAN_ACCEL_Y,
						     &kExpectedValues[1],
						     kExpectedValueShift));
	ASSERT_EQ(0, emul_sensor_backend_set_channel(emul, SENSOR_CHAN_ACCEL_Z,
						     &kExpectedValues[2],
						     kExpectedValueShift));
	ASSERT_EQ(EC_SUCCESS, ms_accel->drv->read(ms_accel, values_mg));

	EXPECT_NEAR(-1750, values_mg[0], 5);
	EXPECT_NEAR(-110, values_mg[1], 5);
	EXPECT_NEAR(1880, values_mg[2], 5);
}

TEST_F(Bmi160, Gyro_Read)
{
	constexpr const int8_t kExpectedValueShift = 3;
	constexpr const q31_t kExpectedValues[] = {
		static_cast<q31_t>(-125.0 * kPi *
				   BIT(31 - kExpectedValueShift) / 180.0),
		static_cast<q31_t>(-7.3 * kPi * BIT(31 - kExpectedValueShift) /
				   180.0),
		static_cast<q31_t>(87.4 * kPi * BIT(31 - kExpectedValueShift) /
				   180.0),
	};
	intv3_t values_mdeg_per_sec;

	ASSERT_EQ(EC_SUCCESS,
		  ms_gyro->drv->set_range(ms_gyro, 250, kRoundDown));
	ASSERT_EQ(0, emul_sensor_backend_set_channel(emul, SENSOR_CHAN_GYRO_X,
						     &kExpectedValues[0],
						     kExpectedValueShift));
	ASSERT_EQ(0, emul_sensor_backend_set_channel(emul, SENSOR_CHAN_GYRO_Y,
						     &kExpectedValues[1],
						     kExpectedValueShift));
	ASSERT_EQ(0, emul_sensor_backend_set_channel(emul, SENSOR_CHAN_GYRO_Z,
						     &kExpectedValues[2],
						     kExpectedValueShift));
	ASSERT_EQ(EC_SUCCESS, ms_gyro->drv->read(ms_gyro, values_mdeg_per_sec));

	EXPECT_NEAR(-125000, values_mdeg_per_sec[0], 5);
	EXPECT_NEAR(-7300, values_mdeg_per_sec[1], 5);
	EXPECT_NEAR(87400, values_mdeg_per_sec[2], 5);
}
