/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>

#include "common.h"
#include "i2c.h"
#include "emul/emul_bmi.h"

#include "driver/accelgyro_bmi160.h"
#include "driver/accelgyro_bmi_common.h"

#define BMI_ORD		DT_DEP_ORD(DT_NODELABEL(accel_bmi260))
#define BMI_SENSOR_ID	SENSOR_ID(DT_NODELABEL(base_accel))

/** How accurate comparision of vectors should be. */
#define V_EPS		8

#define convert_int3v_int16(v, r) do {	\
		r[0] = v[0];		\
		r[1] = v[1];		\
		r[2] = v[2];		\
	} while (0)

/** Rotation used in some tests */
static const mat33_fp_t test_rotation = {
	{ 0, FLOAT_TO_FP(1), 0},
	{ FLOAT_TO_FP(-1), 0, 0},
	{ 0, 0, FLOAT_TO_FP(-1)}
};
/** Rotate given vector by test rotation */
static void rotate_int3v_by_test_rotation(intv3_t v)
{
	int16_t t;
	t = v[0];
	v[0] = -v[1];
	v[1] = t;
	v[2] = -v[2];
}

/** Set emulator offset values to vector of three int16_t */
static void set_emul_offset(struct i2c_emul *emul, intv3_t offset)
{
	bmi_emul_set_off(emul, BMI_EMUL_ACC_X, offset[0]);
	bmi_emul_set_off(emul, BMI_EMUL_ACC_Y, offset[1]);
	bmi_emul_set_off(emul, BMI_EMUL_ACC_Z, offset[2]);
}

/** Save emulator offset values to vector of three int16_t */
static void get_emul_offset(struct i2c_emul *emul, intv3_t offset)
{
	offset[0] = bmi_emul_get_off(emul, BMI_EMUL_ACC_X);
	offset[1] = bmi_emul_get_off(emul, BMI_EMUL_ACC_Y);
	offset[2] = bmi_emul_get_off(emul, BMI_EMUL_ACC_Z);
}

/** Set emulator accelerometer values to vector of three int16_t */
static void set_emul_acc(struct i2c_emul *emul, intv3_t acc)
{
	bmi_emul_set_value(emul, BMI_EMUL_ACC_X, acc[0]);
	bmi_emul_set_value(emul, BMI_EMUL_ACC_Y, acc[1]);
	bmi_emul_set_value(emul, BMI_EMUL_ACC_Z, acc[2]);
}

/** Convert accelerometer read to units used by emulator */
static void drv_acc_to_emul(intv3_t drv, int range, intv3_t out)
{
	const int scale = MOTION_SCALING_FACTOR / BMI_EMUL_1G;
	out[0] = drv[0] * range / scale;
	out[1] = drv[1] * range / scale;
	out[2] = drv[2] * range / scale;
}

/** Compare two vectors of three int16_t */
static void compare_int3v_f(intv3_t exp_v, intv3_t v, int eps, int line)
{
	int i;
	for (i = 0; i < 3; i++) {
		zassert_within(exp_v[i], v[i], eps,
			"Expected [%d; %d; %d], got [%d; %d; %d]; line: %d",
			exp_v[0], exp_v[1], exp_v[2], v[0], v[1], v[2], line);
	}
}
#define compare_int3v_eps(exp_v, v, e) compare_int3v_f(exp_v, v, e, __LINE__)
#define compare_int3v(exp_v, v) compare_int3v_eps(exp_v, v, V_EPS)

/**
 * Test get offset with and without rotation. Also test behaviour on I2C error.
 */
static void test_bmi_get_offset(void)
{
	struct motion_sensor_t *ms;
	struct i2c_emul *emul;
	int16_t ret[3];
	intv3_t ret_v;
	intv3_t exp_v;
	int16_t temp;

	emul = bmi_emul_get(BMI_ORD);
	ms = &motion_sensors[BMI_SENSOR_ID];

	/* Test fail on each axis */
//	bmi_emul_set_read_fail_reg(emul, BMI260_OFFSET_ACC70);
//	zassert_equal(-EIO, ms->drv->get_offset(ms, ret, &temp), NULL);
//	bmi_emul_set_read_fail_reg(emul, BMI260_OFFSET_ACC70 + 1);
//	zassert_equal(-EIO, ms->drv->get_offset(ms, ret, &temp), NULL);
//	bmi_emul_set_read_fail_reg(emul, BMI260_OFFSET_ACC70 + 2);
//	zassert_equal(-EIO, ms->drv->get_offset(ms, ret, &temp), NULL);

	/* Do not fail on read */
	bmi_emul_set_read_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);

	/* Set emulator offset */
	exp_v[0] = BMI_EMUL_1G / 10;
	exp_v[1] = BMI_EMUL_1G / 20;
	exp_v[2] = -(int)BMI_EMUL_1G / 30;
	set_emul_offset(emul, exp_v);
	/* BMI driver returns value in mg units */
	exp_v[0] = 1000 / 10;
	exp_v[1] = 1000 / 20;
	exp_v[2] = -1000 / 30;

	/* Disable rotation */
	ms->rot_standard_ref = NULL;

	/* Test get offset without rotation */
	zassert_equal(EC_SUCCESS, ms->drv->get_offset(ms, ret, &temp),
		      NULL);
	zassert_equal(temp, (int16_t)EC_MOTION_SENSE_INVALID_CALIB_TEMP, NULL);
	convert_int3v_int16(ret, ret_v);
	compare_int3v(exp_v, ret_v);

	/* Setup rotation and rotate expected offset */
	ms->rot_standard_ref = &test_rotation;
	rotate_int3v_by_test_rotation(exp_v);

	/* Test get offset with rotation */
	zassert_equal(EC_SUCCESS, ms->drv->get_offset(ms, ret, &temp),
		      NULL);
	zassert_equal(temp, (int16_t)EC_MOTION_SENSE_INVALID_CALIB_TEMP, NULL);
	convert_int3v_int16(ret, ret_v);
	compare_int3v(exp_v, ret_v);
}

/**
 * Test set offset with and without rotation. Also test behaviour on I2C error.
 */
static void test_bmi_set_offset(void)
{
	struct motion_sensor_t *ms;
	struct i2c_emul *emul;
	int16_t input_v[3];
	int16_t temp = 0;
	intv3_t ret_v;
	intv3_t exp_v;
	uint8_t nv_c;

	emul = bmi_emul_get(BMI_ORD);
	ms = &motion_sensors[BMI_SENSOR_ID];

	/* Test fail on each axis */
//	bma_emul_set_write_fail_reg(emul, BMA2x2_OFFSET_X_AXIS_ADDR);
//	zassert_equal(-EIO, ms.drv->set_offset(&ms, exp_offset, temp), NULL);
//	bma_emul_set_write_fail_reg(emul, BMA2x2_OFFSET_Y_AXIS_ADDR);
//	zassert_equal(-EIO, ms.drv->set_offset(&ms, exp_offset, temp), NULL);
//	bma_emul_set_write_fail_reg(emul, BMA2x2_OFFSET_Z_AXIS_ADDR);
//	zassert_equal(-EIO, ms.drv->set_offset(&ms, exp_offset, temp), NULL);

	/* Test fail on NV CONF register read and write */
	bmi_emul_set_read_fail_reg(emul, BMI260_NV_CONF);
	zassert_equal(-EIO, ms->drv->set_offset(ms, input_v, temp), NULL);
	bmi_emul_set_read_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);
	bmi_emul_set_write_fail_reg(emul, BMI260_NV_CONF);
	zassert_equal(-EIO, ms->drv->set_offset(ms, input_v, temp), NULL);
	bmi_emul_set_write_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);


	/* Setup NV_CONF register value */
	bmi_emul_set_reg(emul, BMI260_NV_CONF, 0x7);
	/* Set input offset */
	exp_v[0] = BMI_EMUL_1G / 10;
	exp_v[1] = BMI_EMUL_1G / 20;
	exp_v[2] = -(int)BMI_EMUL_1G / 30;
	/* BMI driver accept value in mg units */
	input_v[0] = 1000 / 10;
	input_v[1] = 1000 / 20;
	input_v[2] = -1000 / 30;
	/* Disable rotation */
	ms->rot_standard_ref = NULL;

	/* Test set offset without rotation */
	zassert_equal(EC_SUCCESS, ms->drv->set_offset(ms, input_v, temp), NULL);
	get_emul_offset(emul, ret_v);
	/*
	 * Depending on used range, accelerometer values may be up to 6 bits
	 * more accurate then offset value resolution.
	 */
	compare_int3v_eps(exp_v, ret_v, 64);
	nv_c = bmi_emul_get_reg(emul, BMI260_NV_CONF);
	/* Only ACC_OFFSET_EN bit should be changed */
	zassert_equal(0x7 | BMI260_ACC_OFFSET_EN, nv_c,
		      "Expected 0x%x, got 0x%x",
		      0x7 | BMI260_ACC_OFFSET_EN, nv_c);

	/* Setup NV_CONF register value */
	bmi_emul_set_reg(emul, BMI260_NV_CONF, 0);
	/* Setup rotation and rotate input for set_offset function */
	ms->rot_standard_ref = &test_rotation;
	convert_int3v_int16(input_v, ret_v);
	rotate_int3v_by_test_rotation(ret_v);
	convert_int3v_int16(ret_v, input_v);

	/* Test set offset with rotation */
	zassert_equal(EC_SUCCESS, ms->drv->set_offset(ms, input_v, temp), NULL);
	get_emul_offset(emul, ret_v);
	compare_int3v_eps(exp_v, ret_v, 64);
	nv_c = bmi_emul_get_reg(emul, BMI260_NV_CONF);
	/* Only ACC_OFFSET_EN bit should be changed */
	zassert_equal(BMI260_ACC_OFFSET_EN, nv_c, "Expected 0x%x, got 0x%x",
		      BMI260_ACC_OFFSET_EN, nv_c);
}

/*
 * Try to set range and check if expected range was set in driver and in
 * emulator.
 */
static void check_set_acc_range_f(struct i2c_emul *emul,
				  struct motion_sensor_t *ms, int range,
				  int rnd, int exp_range, int line)
{
	uint8_t exp_range_reg;
	uint8_t range_reg;

	zassert_equal(EC_SUCCESS, ms->drv->set_range(ms, range, rnd),
		      "set_range failed; line: %d", line);
	zassert_equal(exp_range, ms->current_range,
		      "Expected range %d, got %d; line %d",
		      exp_range, ms->current_range, line);
	range_reg = bmi_emul_get_reg(emul, BMI260_ACC_RANGE);

	switch (exp_range) {
	case 2:
		exp_range_reg = BMI260_GSEL_2G;
		break;
	case 4:
		exp_range_reg = BMI260_GSEL_4G;
		break;
	case 8:
		exp_range_reg = BMI260_GSEL_8G;
		break;
	case 16:
		exp_range_reg = BMI260_GSEL_16G;
		break;
	default:
		/* Unknown expected range */
		zassert_unreachable(
			"Expected range %d not supported by device; line %d",
			exp_range, line);
		return;
	}

	zassert_equal(exp_range_reg, range_reg,
		      "Expected range reg 0x%x, got 0x%x; line %d",
		      exp_range_reg, range_reg, line);
}
#define check_set_acc_range(emul, ms, range, rnd, exp_range)	\
	check_set_acc_range_f(emul, ms, range, rnd, exp_range, __LINE__)

/** Test set range with and without I2C errors. */
static void test_bmi_set_range(void)
{
	struct motion_sensor_t *ms;
	struct i2c_emul *emul;
	int start_range;

	emul = bmi_emul_get(BMI_ORD);
	ms = &motion_sensors[BMI_SENSOR_ID];

	/* Setup starting range, shouldn't be changed on error */
	start_range = 2;
	ms->current_range = start_range;
	bmi_emul_set_reg(emul, BMI260_ACC_RANGE, BMI260_GSEL_2G);
	/* Setup emulator fail on write */
	bmi_emul_set_write_fail_reg(emul, BMI260_ACC_RANGE);

	/* Test fail on write */
	zassert_equal(-EIO, ms->drv->set_range(ms, 12, 0), NULL);
	zassert_equal(start_range, ms->current_range, NULL);
	zassert_equal(BMI260_GSEL_2G,
		      bmi_emul_get_reg(emul, BMI260_ACC_RANGE), NULL);
	zassert_equal(-EIO, ms->drv->set_range(ms, 12, 1), NULL);
	zassert_equal(start_range, ms->current_range, NULL);
	zassert_equal(BMI260_GSEL_2G,
		      bmi_emul_get_reg(emul, BMI260_ACC_RANGE), NULL);

	/* Do not fail on write */
	bmi_emul_set_write_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);

	/* Test setting range with rounding down */
	check_set_acc_range(emul, ms, 1, 0, 2);
	check_set_acc_range(emul, ms, 2, 0, 2);
	check_set_acc_range(emul, ms, 3, 0, 2);
	check_set_acc_range(emul, ms, 4, 0, 4);
	check_set_acc_range(emul, ms, 5, 0, 4);
	check_set_acc_range(emul, ms, 6, 0, 4);
	check_set_acc_range(emul, ms, 7, 0, 4);
	check_set_acc_range(emul, ms, 8, 0, 8);
	check_set_acc_range(emul, ms, 9, 0, 8);
	check_set_acc_range(emul, ms, 15, 0, 8);
	check_set_acc_range(emul, ms, 16, 0, 16);
	check_set_acc_range(emul, ms, 17, 0, 16);

	/* Test setting range with rounding up */
	check_set_acc_range(emul, ms, 1, 1, 2);
	check_set_acc_range(emul, ms, 2, 1, 2);
	check_set_acc_range(emul, ms, 3, 1, 4);
	check_set_acc_range(emul, ms, 4, 1, 4);
	check_set_acc_range(emul, ms, 5, 1, 8);
	check_set_acc_range(emul, ms, 6, 1, 8);
	check_set_acc_range(emul, ms, 7, 1, 8);
	check_set_acc_range(emul, ms, 8, 1, 8);
	check_set_acc_range(emul, ms, 9, 1, 16);
	check_set_acc_range(emul, ms, 15, 1, 16);
	check_set_acc_range(emul, ms, 16, 1, 16);
	check_set_acc_range(emul, ms, 17, 1, 16);
}

/** Test get resolution. */
static void test_bmi_get_resolution(void)
{
	struct motion_sensor_t *ms;

	ms = &motion_sensors[BMI_SENSOR_ID];

	/* Resolution should be always 16 bits */
	zassert_equal(16, ms->drv->get_resolution(ms), NULL);
}

/*
 * Try to set data rate and check if expected rate was set in driver and in
 * emulator.
 */
static void check_set_acc_rate_f(struct i2c_emul *emul,
				 struct motion_sensor_t *ms, int rate, int rnd,
				 int exp_rate, int line)
{
	uint8_t exp_rate_reg;
	uint8_t rate_reg;
	int drv_rate;

	zassert_equal(EC_SUCCESS, ms->drv->set_data_rate(ms, rate, rnd),
		      "set_data_rate failed; line: %d", line);
	drv_rate = ms->drv->get_data_rate(ms);
	zassert_equal(exp_rate, drv_rate, "Expected rate %d, got %d; line %d",
		      exp_rate, drv_rate, line);
	rate_reg = bmi_emul_get_reg(emul, BMI260_ACC_CONF);
	rate_reg &= BMI_ODR_MASK;

	switch (exp_rate) {
	case 12500:
		exp_rate_reg = 0x5;
		break;
	case 25000:
		exp_rate_reg = 0x6;
		break;
	case 50000:
		exp_rate_reg = 0x7;
		break;
	case 100000:
		exp_rate_reg = 0x8;
		break;
	case 200000:
		exp_rate_reg = 0x9;
		break;
	case 400000:
		exp_rate_reg = 0xa;
		break;
	case 800000:
		exp_rate_reg = 0xb;
		break;
	case 1600000:
		exp_rate_reg = 0xc;
		break;
	default:
		/* Unknown expected rate */
		zassert_unreachable(
			"Expected rate %d not supported by device; line %d",
			exp_rate, line);
		return;
	}

	zassert_equal(exp_rate_reg, rate_reg,
		      "Expected rate reg 0x%x, got 0x%x; line %d",
		      exp_rate_reg, rate_reg, line);
}
#define check_set_acc_rate(emul, ms, rate, rnd, exp_rate)	\
	check_set_acc_rate_f(emul, ms, rate, rnd, exp_rate, __LINE__)

/** Test set and get rate with and without I2C errors. */
static void test_bmi_rate(void)
{
	struct motion_sensor_t *ms;
	struct i2c_emul *emul;
	uint8_t reg_rate;
	uint8_t pwr_ctrl;
	int drv_rate;

	emul = bmi_emul_get(BMI_ORD);
	ms = &motion_sensors[BMI_SENSOR_ID];

	/* Test setting rate with rounding down */
	//check_set_acc_rate(emul, ms, 1, 0, 12500);
	//check_set_acc_rate(emul, ms, 1, 0, 12500);
	//check_set_acc_rate(emul, ms, 12499, 0, 12500);
	check_set_acc_rate(emul, ms, 12500, 0, 12500);
	check_set_acc_rate(emul, ms, 12501, 0, 12500);
	check_set_acc_rate(emul, ms, 24999, 0, 12500);
	check_set_acc_rate(emul, ms, 25000, 0, 25000);
	check_set_acc_rate(emul, ms, 25001, 0, 25000);
	check_set_acc_rate(emul, ms, 49999, 0, 25000);
	check_set_acc_rate(emul, ms, 50000, 0, 50000);
	check_set_acc_rate(emul, ms, 50001, 0, 50000);
	check_set_acc_rate(emul, ms, 99999, 0, 50000);
	check_set_acc_rate(emul, ms, 100000, 0, 100000);
	check_set_acc_rate(emul, ms, 100001, 0, 100000);
	check_set_acc_rate(emul, ms, 199999, 0, 100000);
	check_set_acc_rate(emul, ms, 200000, 0, 200000);
	check_set_acc_rate(emul, ms, 200001, 0, 200000);
	check_set_acc_rate(emul, ms, 399999, 0, 200000);
	/*
	 * We cannot test frequencies from 400000 to 1600000 because
	 * CONFIG_EC_MAX_SENSOR_FREQ_MILLIHZ is set to 250000
	 */

	/* Test setting rate with rounding up */
	check_set_acc_rate(emul, ms, 6251, 1, 12500);
	check_set_acc_rate(emul, ms, 12499, 1, 12500);
	check_set_acc_rate(emul, ms, 12500, 1, 12500);
	check_set_acc_rate(emul, ms, 12501, 1, 25000);
	check_set_acc_rate(emul, ms, 24999, 1, 25000);
	check_set_acc_rate(emul, ms, 25000, 1, 25000);
	check_set_acc_rate(emul, ms, 25001, 1, 50000);
	check_set_acc_rate(emul, ms, 49999, 1, 50000);
	check_set_acc_rate(emul, ms, 50000, 1, 50000);
	check_set_acc_rate(emul, ms, 50001, 1, 100000);
	check_set_acc_rate(emul, ms, 99999, 1, 100000);
	check_set_acc_rate(emul, ms, 100000, 1, 100000);
	check_set_acc_rate(emul, ms, 100001, 1, 200000);
	check_set_acc_rate(emul, ms, 199999, 1, 200000);
	check_set_acc_rate(emul, ms, 200000, 1, 200000);

	/* Test out of range rate with rounding down */
	zassert_equal(EC_RES_INVALID_PARAM,
		      ms->drv->set_data_rate(ms, 1, 0), NULL);
	zassert_equal(EC_RES_INVALID_PARAM,
		      ms->drv->set_data_rate(ms, 12499, 0), NULL);
	zassert_equal(EC_RES_INVALID_PARAM,
		      ms->drv->set_data_rate(ms, 400000, 0), NULL);
	zassert_equal(EC_RES_INVALID_PARAM,
		      ms->drv->set_data_rate(ms, 2000000, 0), NULL);

	/* Test out of range rate with rounding up */
	zassert_equal(EC_RES_INVALID_PARAM,
		      ms->drv->set_data_rate(ms, 1, 1), NULL);
	zassert_equal(EC_RES_INVALID_PARAM,
		      ms->drv->set_data_rate(ms, 6250, 1), NULL);
	zassert_equal(EC_RES_INVALID_PARAM,
		      ms->drv->set_data_rate(ms, 200001, 1), NULL);
	zassert_equal(EC_RES_INVALID_PARAM,
		      ms->drv->set_data_rate(ms, 400000, 1), NULL);
	zassert_equal(EC_RES_INVALID_PARAM,
		      ms->drv->set_data_rate(ms, 2000000, 1), NULL);

	/* Current rate shouldn't be changed on error */
	drv_rate = ms->drv->get_data_rate(ms);
	reg_rate = bmi_emul_get_reg(emul, BMI260_ACC_CONF);

	/* Setup emulator fail on read */
	bmi_emul_set_read_fail_reg(emul, BMI260_ACC_CONF);

	/* Test fail on read */
	zassert_equal(-EIO, ms->drv->set_data_rate(ms, 50000, 0), NULL);
	zassert_equal(drv_rate, ms->drv->get_data_rate(ms), NULL);
	zassert_equal(reg_rate, bmi_emul_get_reg(emul, BMI260_ACC_CONF), NULL);
	zassert_equal(-EIO, ms->drv->set_data_rate(ms, 50000, 1), NULL);
	zassert_equal(drv_rate, ms->drv->get_data_rate(ms), NULL);
	zassert_equal(reg_rate, bmi_emul_get_reg(emul, BMI260_ACC_CONF), NULL);

	/* Do not fail on read */
	bmi_emul_set_read_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);

	/* Setup emulator fail on write */
	bmi_emul_set_write_fail_reg(emul, BMI260_ACC_CONF);

	/* Test fail on write */
	zassert_equal(-EIO, ms->drv->set_data_rate(ms, 50000, 0), NULL);
	zassert_equal(drv_rate, ms->drv->get_data_rate(ms), NULL);
	zassert_equal(reg_rate, bmi_emul_get_reg(emul, BMI260_ACC_CONF), NULL);
	zassert_equal(-EIO, ms->drv->set_data_rate(ms, 50000, 1), NULL);
	zassert_equal(drv_rate, ms->drv->get_data_rate(ms), NULL);
	zassert_equal(reg_rate, bmi_emul_get_reg(emul, BMI260_ACC_CONF), NULL);

	/* Do not fail on write */
	bmi_emul_set_write_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);

	/* Test disabling sensor */
	bmi_emul_set_reg(emul, BMI260_PWR_CTRL,
			 BMI260_AUX_EN | BMI260_GYR_EN | BMI260_ACC_EN);
	bmi_emul_set_reg(emul, BMI260_ACC_CONF, BMI260_FILTER_PERF);
	zassert_equal(EC_SUCCESS, ms->drv->set_data_rate(ms, 0, 0), NULL);

	pwr_ctrl = bmi_emul_get_reg(emul, BMI260_PWR_CTRL);
	reg_rate = bmi_emul_get_reg(emul, BMI260_ACC_CONF);
	zassert_equal(BMI260_AUX_EN | BMI260_GYR_EN, pwr_ctrl, NULL);
	zassert_true(!(reg_rate & BMI260_FILTER_PERF), NULL);

	/* Test enabling sensor */
	bmi_emul_set_reg(emul, BMI260_PWR_CTRL, 0);
	bmi_emul_set_reg(emul, BMI260_ACC_CONF, 0);
	zassert_equal(EC_SUCCESS, ms->drv->set_data_rate(ms, 50000, 0), NULL);

	pwr_ctrl = bmi_emul_get_reg(emul, BMI260_PWR_CTRL);
	reg_rate = bmi_emul_get_reg(emul, BMI260_ACC_CONF);
	zassert_equal(BMI260_ACC_EN, pwr_ctrl, NULL);
	zassert_true(reg_rate & BMI260_FILTER_PERF, NULL);
}

/** Test all simple getters */
static void test_bmi_scale(void)
{
	struct motion_sensor_t *ms;
	int16_t ret_scale[3];
	int16_t exp_scale[3] = {100, 231, 421};
	int16_t t;

	ms = &motion_sensors[BMI_SENSOR_ID];

	zassert_equal(EC_SUCCESS, ms->drv->set_scale(ms, exp_scale, 0), NULL);
	zassert_equal(EC_SUCCESS, ms->drv->get_scale(ms, ret_scale, &t), NULL);

	zassert_equal(t, (int16_t)EC_MOTION_SENSE_INVALID_CALIB_TEMP, NULL);
	zassert_equal(exp_scale[0], ret_scale[0], NULL);
	zassert_equal(exp_scale[1], ret_scale[1], NULL);
	zassert_equal(exp_scale[2], ret_scale[2], NULL);
}

static void test_bmi_read_temp(void)
{
	struct motion_sensor_t *ms;
	struct i2c_emul *emul;
	int ret_temp;
	int exp_temp;

	emul = bmi_emul_get(BMI_ORD);
	ms = &motion_sensors[BMI_SENSOR_ID];

	/* Setup emulator fail on read */
	bmi_emul_set_read_fail_reg(emul, BMI260_TEMPERATURE_0);
	zassert_equal(EC_ERROR_NOT_POWERED, ms->drv->read_temp(ms, &ret_temp),
		      NULL);
	bmi_emul_set_read_fail_reg(emul, BMI260_TEMPERATURE_1);
	zassert_equal(EC_ERROR_NOT_POWERED, ms->drv->read_temp(ms, &ret_temp),
		      NULL);
	/* Do not fail on read */
	bmi_emul_set_read_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);

	/* Fail on invalid temperature */
//	bmi_emul_set_reg(emul, BMI260_TEMPERATURE_0, 0x00);
//	bmi_emul_set_reg(emul, BMI260_TEMPERATURE_1, 0x80);
//	zassert_equal(EC_ERROR_NOT_POWERED, ms->drv->read_temp(ms, &ret_temp),
//		      NULL);

	/* Test correct values */
	exp_temp = 23 + 273;
	bmi_emul_set_reg(emul, BMI260_TEMPERATURE_0, 0x00);
	bmi_emul_set_reg(emul, BMI260_TEMPERATURE_1, 0x00);
	zassert_equal(EC_SUCCESS, ms->drv->read_temp(ms, &ret_temp), NULL);
	zassert_equal(exp_temp, ret_temp, NULL);

	exp_temp = 87 + 273;
	bmi_emul_set_reg(emul, BMI260_TEMPERATURE_0, 0xff);
	bmi_emul_set_reg(emul, BMI260_TEMPERATURE_1, 0x7f);
	zassert_equal(EC_SUCCESS, ms->drv->read_temp(ms, &ret_temp), NULL);
	zassert_equal(exp_temp, ret_temp, "%d", ret_temp);

	exp_temp = -41 + 273;
	bmi_emul_set_reg(emul, BMI260_TEMPERATURE_0, 0x01);
	bmi_emul_set_reg(emul, BMI260_TEMPERATURE_1, 0x80);
	zassert_equal(EC_SUCCESS, ms->drv->read_temp(ms, &ret_temp), NULL);
	zassert_equal(exp_temp, ret_temp, NULL);

	exp_temp = 47 + 273;
	bmi_emul_set_reg(emul, BMI260_TEMPERATURE_0, 0x00);
	bmi_emul_set_reg(emul, BMI260_TEMPERATURE_1, 0x30);
	zassert_equal(EC_SUCCESS, ms->drv->read_temp(ms, &ret_temp), NULL);
	zassert_equal(exp_temp, ret_temp, NULL);
}

/** Test all simple getters */
static void test_bmi_read(void)
{
	struct motion_sensor_t *ms;
	struct i2c_emul *emul;
	intv3_t ret_v;
	intv3_t exp_v;
	int16_t scale[3] = {MOTION_SENSE_DEFAULT_SCALE,
			    MOTION_SENSE_DEFAULT_SCALE,
			    MOTION_SENSE_DEFAULT_SCALE};

	emul = bmi_emul_get(BMI_ORD);
	ms = &motion_sensors[BMI_SENSOR_ID];

	/* Set offset 0 to simplify test */
	bmi_emul_set_off(emul, BMI_EMUL_ACC_X, 0);
	bmi_emul_set_off(emul, BMI_EMUL_ACC_Y, 0);
	bmi_emul_set_off(emul, BMI_EMUL_ACC_Z, 0);

	/* Fail on read status */
	bmi_emul_set_read_fail_reg(emul, BMI260_STATUS);
	zassert_equal(-EIO, ms->drv->read(ms, ret_v), NULL);

	bmi_emul_set_read_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);

	/* When not ready, driver should return saved raw value */
	exp_v[0] = 100;
	exp_v[1] = 200;
	exp_v[2] = 300;
	ms->raw_xyz[0] = exp_v[0];
	ms->raw_xyz[1] = exp_v[1];
	ms->raw_xyz[2] = exp_v[2];

	/* Status not ready */
	bmi_emul_set_reg(emul, BMI260_STATUS, 0);
	zassert_equal(EC_SUCCESS, ms->drv->read(ms, ret_v), NULL);
	compare_int3v(exp_v, ret_v);

	/* Status only GYR ready */
	bmi_emul_set_reg(emul, BMI260_STATUS, BMI260_DRDY_GYR);
	zassert_equal(EC_SUCCESS, ms->drv->read(ms, ret_v), NULL);
	compare_int3v(exp_v, ret_v);

	/* Status ACC ready */
	bmi_emul_set_reg(emul, BMI260_STATUS, BMI260_DRDY_ACC);

	/* Set input accelerometer values */
	exp_v[0] = BMI_EMUL_1G / 10;
	exp_v[1] = BMI_EMUL_1G / 20;
	exp_v[2] = -(int)BMI_EMUL_1G / 30;
	set_emul_acc(emul, exp_v);
	/* Disable rotation */
	ms->rot_standard_ref = NULL;
	/* Set scale */
	zassert_equal(EC_SUCCESS, ms->drv->set_scale(ms, scale, 0), NULL);
	/* Set range to 2G */
	zassert_equal(EC_SUCCESS, ms->drv->set_range(ms, 2, 0), NULL);

	/* Test read without rotation */
	zassert_equal(EC_SUCCESS, ms->drv->read(ms, ret_v), NULL);
	drv_acc_to_emul(ret_v, 2, ret_v);
	compare_int3v(exp_v, ret_v);

	/* Set range to 4G */
	zassert_equal(EC_SUCCESS, ms->drv->set_range(ms, 4, 0), NULL);

	/* Test read without rotation */
	zassert_equal(EC_SUCCESS, ms->drv->read(ms, ret_v), NULL);
	drv_acc_to_emul(ret_v, 4, ret_v);
	compare_int3v(exp_v, ret_v);

	/* Setup rotation and rotate expected vector */
	ms->rot_standard_ref = &test_rotation;
	rotate_int3v_by_test_rotation(exp_v);
	/* Set range to 2G */
	zassert_equal(EC_SUCCESS, ms->drv->set_range(ms, 2, 0), NULL);

	/* Test read with rotation */
	zassert_equal(EC_SUCCESS, ms->drv->read(ms, ret_v), NULL);
	drv_acc_to_emul(ret_v, 2, ret_v);
	compare_int3v(exp_v, ret_v);

	/* Set range to 4G */
	zassert_equal(EC_SUCCESS, ms->drv->set_range(ms, 4, 0), NULL);

	/* Test read with rotation */
	zassert_equal(EC_SUCCESS, ms->drv->read(ms, ret_v), NULL);
	drv_acc_to_emul(ret_v, 4, ret_v);
	compare_int3v(exp_v, ret_v);

	/* Fail on read of data registers */
	bmi_emul_set_read_fail_reg(emul, BMI260_ACC_X_L_G);
	zassert_equal(-EIO, ms->drv->read(ms, ret_v), NULL);
	bmi_emul_set_read_fail_reg(emul, BMI260_ACC_X_H_G);
	zassert_equal(-EIO, ms->drv->read(ms, ret_v), NULL);
	bmi_emul_set_read_fail_reg(emul, BMI260_ACC_Y_L_G);
	zassert_equal(-EIO, ms->drv->read(ms, ret_v), NULL);
	bmi_emul_set_read_fail_reg(emul, BMI260_ACC_Y_H_G);
	zassert_equal(-EIO, ms->drv->read(ms, ret_v), NULL);
	bmi_emul_set_read_fail_reg(emul, BMI260_ACC_Z_L_G);
	zassert_equal(-EIO, ms->drv->read(ms, ret_v), NULL);
	bmi_emul_set_read_fail_reg(emul, BMI260_ACC_Z_H_G);
	zassert_equal(-EIO, ms->drv->read(ms, ret_v), NULL);

	bmi_emul_set_read_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);
	ms->rot_standard_ref = NULL;
}

/** Test offset compensation with and without I2C errors. */
static void test_bmi_perform_calib(void)
{
	struct motion_sensor_t *ms;
	struct i2c_emul *emul;
	intv3_t start_off;
	intv3_t exp_off;
	intv3_t ret_off;
	int range;
	int rate;

	emul = bmi_emul_get(BMI_ORD);
	ms = &motion_sensors[BMI_SENSOR_ID];

	/* Range and rate cannot change after calibration */
	range = 4;
	rate = 50000;
	zassert_equal(EC_SUCCESS, ms->drv->set_range(ms, range, 0), NULL);
	zassert_equal(EC_SUCCESS, ms->drv->set_data_rate(ms, rate, 0), NULL);

	/* Set offset 0 */
	start_off[0] = 0;
	start_off[1] = 0;
	start_off[2] = 0;
	set_emul_offset(emul, start_off);

	/* Set input accelerometer values */
	exp_off[0] = BMI_EMUL_1G / 10;
	exp_off[1] = BMI_EMUL_1G / 20;
	exp_off[2] = BMI_EMUL_1G - (int)BMI_EMUL_1G / 30;
	set_emul_acc(emul, exp_off);

	/* Expected offset is [-X, -Y, 1G - Z] */
	exp_off[0] = -exp_off[0];
	exp_off[1] = -exp_off[1];
	exp_off[2] = BMI_EMUL_1G - exp_off[2];

	/* Setup emulator calibration functions */
	//bma_emul_set_read_func(emul, emul_read_calib_func, &func_data);
	//bma_emul_set_write_func(emul, emul_write_calib_func, &func_data);

	/* Setup emulator to fail on first access to offset control register */
	//func_data.calib_start = k_uptime_get_32();
	//func_data.read_fail = 1;
	//func_data.time = 1000000;

	/* Test success on disabling calibration */
	zassert_equal(EC_SUCCESS, ms->drv->perform_calib(ms, 0), NULL);
	zassert_equal(range, ms->current_range, NULL);
	zassert_equal(rate, ms->drv->get_data_rate(ms), NULL);

	/* Test fail on rate read */
	bmi_emul_set_read_fail_reg(emul, BMI260_ACC_CONF);
	zassert_equal(-EIO, ms->drv->perform_calib(ms, 1), NULL);
	zassert_equal(range, ms->current_range, NULL);
	bmi_emul_set_read_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);
	zassert_equal(rate, ms->drv->get_data_rate(ms), NULL);

	/* Test fail on status read */
	bmi_emul_set_read_fail_reg(emul, BMI260_STATUS);
	zassert_equal(-EIO, ms->drv->perform_calib(ms, 1), NULL);
	zassert_equal(range, ms->current_range, NULL);
	zassert_equal(rate, ms->drv->get_data_rate(ms), NULL);

	/* Test fail on data not ready */
	bmi_emul_set_read_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);
	bmi_emul_set_reg(emul, BMI260_STATUS, 0);
	zassert_equal(EC_ERROR_TIMEOUT, ms->drv->perform_calib(ms, 1), NULL);
	zassert_equal(range, ms->current_range, NULL);
	zassert_equal(rate, ms->drv->get_data_rate(ms), NULL);

	/* Setup data status ready for rest of the test */
	bmi_emul_set_reg(emul, BMI260_STATUS, BMI260_DRDY_ACC);

	/* Test fail on data read */
	bmi_emul_set_read_fail_reg(emul, BMI260_ACC_X_L_G);
	zassert_equal(-EIO, ms->drv->perform_calib(ms, 1), NULL);
	zassert_equal(range, ms->current_range, NULL);
	zassert_equal(rate, ms->drv->get_data_rate(ms), NULL);

	/* Test fail on setting offset */
	bmi_emul_set_read_fail_reg(emul, BMI260_NV_CONF);
	zassert_equal(-EIO, ms->drv->perform_calib(ms, 1), NULL);
	zassert_equal(range, ms->current_range, NULL);
	zassert_equal(rate, ms->drv->get_data_rate(ms), NULL);

	bmi_emul_set_read_fail_reg(emul, BMI_EMUL_NO_FAIL_REG);

	/* Test successful offset compenastion */
	zassert_equal(EC_SUCCESS, ms->drv->perform_calib(ms, 1), NULL);
	zassert_equal(range, ms->current_range, NULL);
	zassert_equal(rate, ms->drv->get_data_rate(ms), NULL);
	get_emul_offset(emul, ret_off);
	/*
	 * Depending on used range, accelerometer values may be up to 6 bits
	 * more accurate then offset value resolution.
	 */
	compare_int3v_eps(exp_off, ret_off, 64);
}

void test_suite_bmi260(void)
{
	ztest_test_suite(bmi260,
			 ztest_user_unit_test(test_bmi_get_offset),
			 ztest_user_unit_test(test_bmi_set_offset),
			 ztest_user_unit_test(test_bmi_set_range),
			 ztest_user_unit_test(test_bmi_get_resolution),
			 ztest_user_unit_test(test_bmi_rate),
			 ztest_user_unit_test(test_bmi_scale),
			 ztest_user_unit_test(test_bmi_read_temp),
			 ztest_user_unit_test(test_bmi_read),
			 ztest_user_unit_test(test_bmi_perform_calib));
	ztest_run_test_suite(bmi260);
}
