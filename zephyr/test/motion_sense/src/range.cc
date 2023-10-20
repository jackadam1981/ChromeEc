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

namespace
{
constexpr int kRoundDown = 0;
constexpr int kRoundUp = 1;
} /* namespace */

TEST_F(Bmi160, Accel_SetRange)
{
	uint8_t reg_value;

	for (int range_g = 2; range_g <= 16; range_g *= 2) {
		EXPECT_EQ(EC_SUCCESS, ms_accel->drv->set_range(
					      ms_accel, range_g, kRoundDown))
			<< "range=" << range_g << ", rounding DOWN";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_ACC_RANGE, &reg_value, 1))
			<< "range=" << range_g << ", rounding DOWN";
		EXPECT_EQ(range_g, bmi160_acc_reg_val_to_range(reg_value))
			<< "reg_value=" << reg_value;

		EXPECT_EQ(EC_SUCCESS,
			  ms_accel->drv->set_range(ms_accel, range_g + 1,
						   kRoundDown))
			<< "range=" << range_g + 1 << ", rounding DOWN";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_ACC_RANGE, &reg_value, 1))
			<< "range=" << range_g + 1 << ", rounding DOWN";
		EXPECT_EQ(range_g, bmi160_acc_reg_val_to_range(reg_value))
			<< "range=" << range_g + 1
			<< ", reg_value=" << reg_value;

		EXPECT_EQ(EC_SUCCESS, ms_accel->drv->set_range(
					      ms_accel, range_g - 1, kRoundUp))
			<< "range=" << range_g - 1 << ", rounding UP";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_ACC_RANGE, &reg_value, 1))
			<< "range=" << range_g - 1 << ", rounding UP";
		EXPECT_EQ(range_g, bmi160_acc_reg_val_to_range(reg_value))
			<< "range=" << range_g - 1
			<< ", reg_value=" << reg_value;
	}
}

TEST_F(Bmi160, Gyro_SetRange)
{
	uint8_t reg_value;

	for (int range_deg_per_sec = 125; range_deg_per_sec <= 2000;
	     range_deg_per_sec *= 2) {
		EXPECT_EQ(EC_SUCCESS,
			  ms_gyro->drv->set_range(ms_gyro, range_deg_per_sec,
						  kRoundDown))
			<< "range=" << range_deg_per_sec << ", rounding DOWN";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_GYR_RANGE, &reg_value, 1))
			<< "range=" << range_deg_per_sec << ", rounding DOWN";
		EXPECT_EQ(range_deg_per_sec,
			  bmi160_gyr_reg_val_to_range(reg_value))
			<< "reg_value=" << reg_value;

		EXPECT_EQ(EC_SUCCESS,
			  ms_gyro->drv->set_range(ms_gyro, range_deg_per_sec + 1,
						  kRoundDown))
			<< "range=" << range_deg_per_sec + 1 << ", rounding DOWN";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_GYR_RANGE, &reg_value, 1))
			<< "range=" << range_deg_per_sec + 1 << ", rounding DOWN";
		EXPECT_EQ(range_deg_per_sec,
			  bmi160_gyr_reg_val_to_range(reg_value))
			<< "range=" << range_deg_per_sec + 1
			<< "reg_value=" << reg_value;

		EXPECT_EQ(EC_SUCCESS,
			  ms_gyro->drv->set_range(ms_gyro, range_deg_per_sec - 1,
						  kRoundUp))
			<< "range=" << range_deg_per_sec - 1 << ", rounding UP";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_GYR_RANGE, &reg_value, 1))
			<< "range=" << range_deg_per_sec - 1 << ", rounding UP";
		EXPECT_EQ(range_deg_per_sec,
			  bmi160_gyr_reg_val_to_range(reg_value))
			<< "range=" << range_deg_per_sec - 1
			<< "reg_value=" << reg_value;
	}
}
