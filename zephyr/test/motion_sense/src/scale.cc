/* Copyright 2023 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "bmi160.h"
#include "emul_bmi160.h"
#include "math_util.h"
#include "test/motion_sense/fixtures.h"

#include <gtest/gtest.h>

TEST_F(Bmi160, Accel_SetGetScale)
{
	const uint16_t expected_scale[] = { 1, 15, 20 };
	uint16_t scale[3];
	int16_t temperature;

	EXPECT_EQ(EC_SUCCESS,
		  ms_accel->drv->set_scale(ms_accel, expected_scale, 0));
	EXPECT_EQ(EC_SUCCESS,
		  ms_accel->drv->get_scale(ms_accel, scale, &temperature));
	EXPECT_EQ(expected_scale[0], scale[0]);
	EXPECT_EQ(expected_scale[1], scale[1]);
	EXPECT_EQ(expected_scale[2], scale[2]);
	EXPECT_EQ(EC_MOTION_SENSE_INVALID_CALIB_TEMP, temperature);
}

TEST_F(Bmi160, Gyro_SetGetScale)
{
	const uint16_t expected_scale[] = { 17, 3, 33 };
	uint16_t scale[3];
	int16_t temperature;

	EXPECT_EQ(EC_SUCCESS,
		  ms_accel->drv->set_scale(ms_accel, expected_scale, 0));
	EXPECT_EQ(EC_SUCCESS,
		  ms_accel->drv->get_scale(ms_accel, scale, &temperature));
	EXPECT_EQ(expected_scale[0], scale[0]);
	EXPECT_EQ(expected_scale[1], scale[1]);
	EXPECT_EQ(expected_scale[2], scale[2]);
	EXPECT_EQ(EC_MOTION_SENSE_INVALID_CALIB_TEMP, temperature);
}
