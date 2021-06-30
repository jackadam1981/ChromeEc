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

/** Rotation used in some tests */
static const mat33_fp_t test_rotation = {
	{ 0, FLOAT_TO_FP(1), 0},
	{ FLOAT_TO_FP(-1), 0, 0},
	{ 0, 0, FLOAT_TO_FP(-1)}
};
/** Rotate given vector by test rotation */
void rotate_int3v_by_test_rotation(intv3_t v)
{
	int16_t t;
	t = v[0];
	v[0] = -v[1];
	v[1] = t;
	v[2] = -v[2];
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
static void compare_int3v_f(intv3_t exp_v, intv3_t v, int line)
{
	int i;
	for (i = 0; i < 3; i++) {
		zassert_within(exp_v[i], v[i], V_EPS,
			"Expected [%d; %d; %d], got [%d; %d; %d]; line: %d",
			exp_v[0], exp_v[1], exp_v[2], v[0], v[1], v[2], line);
	}
}
#define compare_int3v(exp_v, v) compare_int3v_f(exp_v, v, __LINE__)

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
}

void test_suite_bmi260(void)
{
	ztest_test_suite(bmi260,
			 ztest_user_unit_test(test_bmi_read));
	ztest_run_test_suite(bmi260);
}
