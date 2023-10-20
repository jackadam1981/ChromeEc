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

TEST_F(Bmi160, Accel_SetDataRate)
{
	uint8_t reg_value;

	for (int expected_reg_value = 1; expected_reg_value <= 12;
	     ++expected_reg_value) {
		int rate = 25000 * BIT(expected_reg_value - 1) / 32;

		EXPECT_EQ(EC_SUCCESS, ms_accel->drv->set_data_rate(
					      ms_accel, rate, kRoundDown))
			<< "rate=" << rate << ", rounding DOWN";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_ACC_CONF, &reg_value, 1))
			<< "rate=" << rate << ", rounding DOWN";
		EXPECT_EQ(expected_reg_value,
			  FIELD_GET(BMI160_ACC_CONF_ODR_MASK, reg_value))
			<< "rate=" << rate << ", rounding DOWN";

		EXPECT_EQ(EC_SUCCESS, ms_accel->drv->set_data_rate(
					      ms_accel, rate + 1, kRoundDown))
			<< "rate=" << rate + 1 << ", rounding DOWN";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_ACC_CONF, &reg_value, 1))
			<< "rate=" << rate + 1 << ", rounding DOWN";
		EXPECT_EQ(expected_reg_value,
			  FIELD_GET(BMI160_ACC_CONF_ODR_MASK, reg_value))
			<< "rate=" << rate + 1 << ", rounding DOWN";

		EXPECT_EQ(EC_SUCCESS, ms_accel->drv->set_data_rate(
					      ms_accel, rate - 1, kRoundUp))
			<< "rate=" << rate - 1 << ", rounding UP";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_ACC_CONF, &reg_value, 1))
			<< "rate=" << rate - 1 << ", rounding UP";
		EXPECT_EQ(expected_reg_value,
			  FIELD_GET(BMI160_ACC_CONF_ODR_MASK, reg_value))
			<< "rate=" << rate - 1 << ", rounding UP";
	}
}

TEST_F(Bmi160, Accel_GetDataRate)
{
	for (int expected_reg_value = 1; expected_reg_value <= 12;
	     ++expected_reg_value) {
		int rate = 25000 * BIT(expected_reg_value - 1) / 32;

		EXPECT_EQ(EC_SUCCESS, ms_accel->drv->set_data_rate(
					      ms_accel, rate, kRoundDown))
			<< "rate=" << rate << ", rounding DOWN";
		EXPECT_EQ(rate, ms_accel->drv->get_data_rate(ms_accel))
			<< "rate=" << rate << ", rounding DOWN";
	}
}

TEST_F(Bmi160, Gyro_SetDataRate)
{
	uint8_t reg_value;

	for (int expected_reg_value = 6; expected_reg_value <= 13;
	     ++expected_reg_value) {
		int rate = 100000 * BIT(expected_reg_value) / 256;

		EXPECT_EQ(EC_SUCCESS, ms_gyro->drv->set_data_rate(ms_gyro, rate,
								  kRoundDown))
			<< "rate=" << rate << ", rounding DOWN";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_GYR_CONF, &reg_value, 1))
			<< "rate=" << rate << ", rounding DOWN";
		EXPECT_EQ(expected_reg_value,
			  FIELD_GET(BMI160_GYR_CONF_ODR_MASK, reg_value))
			<< "rate=" << rate << ", rounding DOWN";

		EXPECT_EQ(EC_SUCCESS, ms_gyro->drv->set_data_rate(
					      ms_gyro, rate + 1, kRoundDown))
			<< "rate=" << rate + 1 << ", rounding DOWN";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_GYR_CONF, &reg_value, 1))
			<< "rate=" << rate + 1 << ", rounding DOWN";
		EXPECT_EQ(expected_reg_value,
			  FIELD_GET(BMI160_GYR_CONF_ODR_MASK, reg_value))
			<< "rate=" << rate + 1 << ", rounding DOWN";

		EXPECT_EQ(EC_SUCCESS, ms_gyro->drv->set_data_rate(
					      ms_gyro, rate - 1, kRoundUp))
			<< "rate=" << rate - 1 << ", rounding UP";
		EXPECT_EQ(0, emul_bmi160_get_reg_value(
				     emul, BMI160_REG_GYR_CONF, &reg_value, 1))
			<< "rate=" << rate - 1 << ", rounding UP";
		EXPECT_EQ(expected_reg_value,
			  FIELD_GET(BMI160_GYR_CONF_ODR_MASK, reg_value))
			<< "rate=" << rate - 1 << ", rounding UP";
	}
}

TEST_F(Bmi160, Gyro_GetDataRate)
{
	for (int expected_reg_value = 6; expected_reg_value <= 13;
	     ++expected_reg_value) {
		int rate = 100000 * BIT(expected_reg_value) / 256;

		EXPECT_EQ(EC_SUCCESS, ms_gyro->drv->set_data_rate(ms_gyro, rate,
								  kRoundDown))
			<< "rate=" << rate << ", rounding DOWN";
		EXPECT_EQ(rate, ms_gyro->drv->get_data_rate(ms_gyro))
			<< "rate=" << rate << ", rounding DOWN";
	}
}
