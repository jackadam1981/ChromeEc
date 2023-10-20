/* Copyright 2023 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "bmi160.h"
#include "emul_bmi160.h"
#include "math_util.h"
#include "test/motion_sense/fixtures.h"

#include <zephyr/drivers/sensor_attribute_types.h>

#include <gtest/gtest.h>

TEST_F(Bmi160, Accel_GetOffset)
{
	struct sensor_three_axis_attribute offset = {
		.shift = 0,
		.x = round_divide(9.80665f * (int64_t)INT32_MAX, 10),
		.y = round_divide(9.80665f * (int64_t)INT32_MAX, 20),
		.z = round_divide(-9.80665f * (int64_t)INT32_MAX, 30),
	};

	/* Set emulator offset */
	ASSERT_EQ(0, emul_sensor_backend_set_attribute(
			     emul, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_OFFSET,
			     &offset));

	/* Query the chip's accuracy */
	int epsilon_mg = get_accel_offset_epsilon(emul);

	int16_t ret_offsets[3];
	int16_t temperature;

	ASSERT_EQ(EC_SUCCESS, ms_accel->drv->get_offset(ms_accel, ret_offsets,
							&temperature));
	EXPECT_EQ(EC_MOTION_SENSE_INVALID_CALIB_TEMP, temperature);
	EXPECT_NEAR(100, ret_offsets[0], epsilon_mg);
	EXPECT_NEAR(50, ret_offsets[1], epsilon_mg);
	EXPECT_NEAR(-33, ret_offsets[2], epsilon_mg);

	const mat33_fp_t test_rotation = { { 0, FLOAT_TO_FP(1), 0 },
					   { FLOAT_TO_FP(-1), 0, 0 },
					   { 0, 0, FLOAT_TO_FP(-1) } };

	ms_accel->rot_standard_ref = &test_rotation;

	ASSERT_EQ(EC_SUCCESS, ms_accel->drv->get_offset(ms_accel, ret_offsets,
							&temperature));
	EXPECT_EQ(EC_MOTION_SENSE_INVALID_CALIB_TEMP, temperature);
	EXPECT_NEAR(-50, ret_offsets[0], epsilon_mg);
	EXPECT_NEAR(100, ret_offsets[1], epsilon_mg);
	EXPECT_NEAR(33, ret_offsets[2], epsilon_mg);
}

TEST_F(Bmi160, Gyro_GetOffset)
{
	struct sensor_three_axis_attribute offset = {
		.shift = 0,
		.x = round_divide(INT64_C(125) * 3.141593f * (int64_t)INT32_MAX,
				  180 * 100),
		.y = round_divide(INT64_C(125) * 3.141593f * (int64_t)INT32_MAX,
				  180 * 200),
		.z = round_divide(INT64_C(-125) * 3.141593f *
					  (int64_t)INT32_MAX,
				  180 * 300),
	};

	/* Set emulator offset */
	ASSERT_EQ(0, emul_sensor_backend_set_attribute(
			     emul, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_OFFSET,
			     &offset));

	/* Query the chip's accuracy */
	int epsilon_mdeg_per_s = get_gyro_offset_epsilon(emul);

	int16_t ret_offsets[3];
	int16_t temperature;

	ASSERT_EQ(EC_SUCCESS,
		  ms_gyro->drv->get_offset(ms_gyro, ret_offsets, &temperature));
	EXPECT_EQ(EC_MOTION_SENSE_INVALID_CALIB_TEMP, temperature);
	EXPECT_NEAR(1250, ret_offsets[0], epsilon_mdeg_per_s);
	EXPECT_NEAR(625, ret_offsets[1], epsilon_mdeg_per_s);
	EXPECT_NEAR(-416, ret_offsets[2], epsilon_mdeg_per_s);

	const mat33_fp_t test_rotation = { { 0, FLOAT_TO_FP(1), 0 },
					   { FLOAT_TO_FP(-1), 0, 0 },
					   { 0, 0, FLOAT_TO_FP(-1) } };

	ms_gyro->rot_standard_ref = &test_rotation;

	ASSERT_EQ(EC_SUCCESS,
		  ms_gyro->drv->get_offset(ms_gyro, ret_offsets, &temperature));
	EXPECT_EQ(EC_MOTION_SENSE_INVALID_CALIB_TEMP, temperature);
	EXPECT_NEAR(-625, ret_offsets[0], epsilon_mdeg_per_s);
	EXPECT_NEAR(1250, ret_offsets[1], epsilon_mdeg_per_s);
	EXPECT_NEAR(416, ret_offsets[2], epsilon_mdeg_per_s);
}

TEST_F(Bmi160, Accel_SetOffset)
{
	/* BMI driver accept value in mg units */
	int16_t input_v[3] = { 1000 / 10, 1000 / 20, -1000 / 30 };
	uint8_t reg_values[7];
	int16_t temp = 0;

	ASSERT_EQ(EC_SUCCESS,
		  ms_accel->drv->set_offset(ms_accel, input_v, temp));
	ASSERT_EQ(0, emul_bmi160_get_reg_value(emul, BMI160_REG_OFFSET_ACC_X,
					       reg_values, 7));
	EXPECT_EQ(0x19, reg_values[0]);
	EXPECT_EQ(0x0c, reg_values[1]);
	EXPECT_EQ(0xf8, reg_values[2]);
	EXPECT_EQ(0x00, reg_values[3]);
	EXPECT_EQ(0x00, reg_values[4]);
	EXPECT_EQ(0x00, reg_values[5]);
	EXPECT_EQ(BIT(BMI160_ACC_OFS_EN_POS), reg_values[6]);
}

TEST_F(Bmi160, Accel_SetOffset_MinMax)
{
	/* BMI driver accept value in mg units */
	int16_t input_v[3] = { INT16_MAX, INT16_MIN, 0 };
	uint8_t reg_values[7];
	int16_t temp = 0;

	ASSERT_EQ(EC_SUCCESS,
		  ms_accel->drv->set_offset(ms_accel, input_v, temp));
	ASSERT_EQ(0, emul_bmi160_get_reg_value(emul, BMI160_REG_OFFSET_ACC_X,
					       reg_values, 7));
	EXPECT_EQ(0x7f, reg_values[0]);
	EXPECT_EQ(0x80, reg_values[1]);
	EXPECT_EQ(0x00, reg_values[2]);
	EXPECT_EQ(0x00, reg_values[3]);
	EXPECT_EQ(0x00, reg_values[4]);
	EXPECT_EQ(0x00, reg_values[5]);
	EXPECT_EQ(BIT(BMI160_ACC_OFS_EN_POS), reg_values[6]);
}

TEST_F(Bmi160, Gyro_SetOffset)
{
	/* BMI driver accept value in mdeg/s units */
	int16_t input_v[3] = { 125000 / 100, 125000 / 200, -125000 / 300 };
	uint8_t reg_values[7];
	int16_t temp = 0;

	ASSERT_EQ(EC_SUCCESS, ms_gyro->drv->set_offset(ms_gyro, input_v, temp));
	ASSERT_EQ(0, emul_bmi160_get_reg_value(emul, BMI160_REG_OFFSET_ACC_X,
					       reg_values, 7));
	EXPECT_EQ(0x00, reg_values[0]);
	EXPECT_EQ(0x00, reg_values[1]);
	EXPECT_EQ(0x00, reg_values[2]);
	EXPECT_EQ(0x14, reg_values[3]);
	EXPECT_EQ(0x0a, reg_values[4]);
	EXPECT_EQ(0xfa, reg_values[5]);
	EXPECT_EQ(1, FIELD_GET(BIT(BMI160_GYR_OFS_EN_POS), reg_values[6]));
	EXPECT_EQ(0x03, FIELD_GET(BMI160_GYR_MSB_OFS_Z_MASK, reg_values[6]));
}

TEST_F(Bmi160, Gyro_SetOffset_MinMax)
{
	/* BMI driver accept value in mdeg/s units */
	int16_t input_v[3] = { INT16_MAX, INT16_MIN, 0 };
	uint8_t reg_values[7];
	int16_t temp = 0;

	ASSERT_EQ(EC_SUCCESS, ms_gyro->drv->set_offset(ms_gyro, input_v, temp));
	ASSERT_EQ(0, emul_bmi160_get_reg_value(emul, BMI160_REG_OFFSET_ACC_X,
					       reg_values, 7));
	EXPECT_EQ(0x00, reg_values[0]);
	EXPECT_EQ(0x00, reg_values[1]);
	EXPECT_EQ(0x00, reg_values[2]);
	EXPECT_EQ(0xff, reg_values[3]);
	EXPECT_EQ(0x00, reg_values[4]);
	EXPECT_EQ(0x00, reg_values[5]);
	EXPECT_EQ(1, FIELD_GET(BIT(BMI160_GYR_OFS_EN_POS), reg_values[6]));
	EXPECT_EQ(0x02, FIELD_GET(BMI160_GYR_MSB_OFS_Y_MASK, reg_values[6]));
}
