/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>

#include "common.h"
#include "i2c.h"
#include "emul/emul_bma255.h"

#include "accelgyro.h"
#include "motion_sense.h"
#include "driver/accel_bma2x2.h"

/* How accurate conversions should be */
#define G_EPS 0.1 
#define V_EPS 8

#define EMUL_LABEL DT_NODELABEL(bma_emul)

#define BMA_ORD	DT_DEP_ORD(EMUL_LABEL)

/* Mutex for test motion sensor  */
static mutex_t sensor_mutex;
/* Rotation used in some tests */
static const mat33_fp_t test_rotation = {
	{ 0, FLOAT_TO_FP(1), 0},
	{ FLOAT_TO_FP(-1), 0, 0},
	{ 0, 0, FLOAT_TO_FP(-1)}
};
/** Rotate given vector by test rotation */
void rotate_int3v_by_test_rotation(int16_t *v)
{
	int16_t t;

	t = v[0];
	v[0] = -v[1];
	v[1] = t;
	v[2] = -v[2];
}

static struct accelgyro_saved_data_t acc_data;

/** Mock minimal motion sensor setup required for bma2x2 driver test */
static struct motion_sensor_t ms = {
	.name = "bma_emul",
	.type = MOTIONSENSE_TYPE_ACCEL,
	.drv = &bma2x2_accel_drv,
	.mutex = &sensor_mutex,
	.drv_data = &acc_data,
	.port = NAMED_I2C(accel),
	.i2c_spi_addr_flags = DT_REG_ADDR(EMUL_LABEL),
	.rot_standard_ref = NULL,
	.current_range = 0,
};

static void set_emul_offset(struct i2c_emul *emul, int16_t *offset)
{
	bma_emul_set_off(emul, BMA_EMUL_AXIS_X, offset[0]);
	bma_emul_set_off(emul, BMA_EMUL_AXIS_Y, offset[1]);
	bma_emul_set_off(emul, BMA_EMUL_AXIS_Z, offset[2]);
}

static void get_emul_offset(struct i2c_emul *emul, int16_t *offset)
{
	offset[0] = bma_emul_get_off(emul, BMA_EMUL_AXIS_X);
	offset[1] = bma_emul_get_off(emul, BMA_EMUL_AXIS_Y);
	offset[2] = bma_emul_get_off(emul, BMA_EMUL_AXIS_Z);
}

static float emul_to_g(int16_t val)
{
	return ((float)val) / (float)BMA_EMUL_1G;
}

static float accelgyro_to_g(int16_t val)
{
	return ((float)val) / (float)MOTION_SCALING_FACTOR;
}

static void compare_int3v_f(int16_t *exp_v, int16_t *v, int line)
{
	int i;

	for (i = 0; i < 3; i++) {
		zassert_within(exp_v[i], v[i], V_EPS,
			"Expected [%d; %d; %d], got [%d; %d; %d]; line: %d",
			exp_v[0], exp_v[1], exp_v[2], v[0], v[1], v[2], line);
	}
}
#define compare_int3v(exp_v, v) compare_int3v_f(exp_v, v, __LINE__)

/** Data for fail functions */
struct fail_func_data {
	/** Fail for given address or for all if -1 */
	int reg;
};

static int emul_read_func_fail(struct i2c_emul *emul, int reg, void *data)
{
	struct fail_func_data *d = data;

	if (d->reg == -1 || d->reg == reg) {
		return -EIO;
	}

	return 1;
}

static int emul_write_func_fail(struct i2c_emul *emul, int reg, uint8_t val,
				void *data)
{
	struct fail_func_data *d = data;

	if (d->reg == -1 || d->reg == reg) {
		return -EIO;
	}

	return 1;
}

/**
 * Test get offset with and without rotation. Also test behaviour on I2C error.
 */
static void test_bma_get_offset(void)
{
	struct fail_func_data func_data;
	struct i2c_emul *emul;
	int16_t ret_offset[3];
	int16_t exp_offset[3];
	int16_t temp;

	emul = bma_emul_get(BMA_ORD);

	/* Setup emulator fail read function */
	bma_emul_set_read_func(emul, emul_read_func_fail, &func_data);

	/* Test fail on each axis */
	func_data.reg = BMA2x2_OFFSET_X_AXIS_ADDR;
	zassert_equal(-EIO, ms.drv->get_offset(&ms, ret_offset, &temp), NULL);
	func_data.reg = BMA2x2_OFFSET_Y_AXIS_ADDR;
	zassert_equal(-EIO, ms.drv->get_offset(&ms, ret_offset, &temp), NULL);
	func_data.reg = BMA2x2_OFFSET_Z_AXIS_ADDR;
	zassert_equal(-EIO, ms.drv->get_offset(&ms, ret_offset, &temp), NULL);

	/* Remove custom emulator read function */
	bma_emul_set_read_func(emul, NULL, NULL);

	/* Set emulator offset */
	exp_offset[0] = BMA_EMUL_1G / 10;
	exp_offset[1] = BMA_EMUL_1G / 20;
	exp_offset[2] = -(int)BMA_EMUL_1G / 30;
	set_emul_offset(emul, exp_offset);
	/* Disable rotation */
	ms.rot_standard_ref = NULL;

	/* Test get offset without rotation */
	zassert_equal(EC_SUCCESS, ms.drv->get_offset(&ms, ret_offset, &temp),
		      NULL);
	zassert_equal(temp, (int16_t)EC_MOTION_SENSE_INVALID_CALIB_TEMP, NULL);
	compare_int3v(exp_offset, ret_offset);

	/* Setup rotation and rotate expected offset */
	ms.rot_standard_ref = &test_rotation;
	rotate_int3v_by_test_rotation(exp_offset);

	/* Test get offset with rotation */
	zassert_equal(EC_SUCCESS, ms.drv->get_offset(&ms, ret_offset, &temp),
		      NULL);
	zassert_equal(temp, (int16_t)EC_MOTION_SENSE_INVALID_CALIB_TEMP, NULL);
	compare_int3v(exp_offset, ret_offset);
}

/**
 * Test set offset with and without rotation. Also test behaviour on I2C error.
 */
static void test_bma_set_offset(void)
{
	struct fail_func_data func_data;
	struct i2c_emul *emul;
	int16_t ret_offset[3];
	int16_t exp_offset[3];
	int16_t temp = 0;

	emul = bma_emul_get(BMA_ORD);

	/* Setup emulator fail write function */
	bma_emul_set_write_func(emul, emul_write_func_fail, &func_data);

	/* Test fail on each axis */
	func_data.reg = BMA2x2_OFFSET_X_AXIS_ADDR;
	zassert_equal(-EIO, ms.drv->set_offset(&ms, exp_offset, temp), NULL);
	func_data.reg = BMA2x2_OFFSET_Y_AXIS_ADDR;
	zassert_equal(-EIO, ms.drv->set_offset(&ms, exp_offset, temp), NULL);
	func_data.reg = BMA2x2_OFFSET_Z_AXIS_ADDR;
	zassert_equal(-EIO, ms.drv->set_offset(&ms, exp_offset, temp), NULL);

	/* Remove custom emulator read function */
	bma_emul_set_write_func(emul, NULL, NULL);

	/* Set input offset */
	exp_offset[0] = BMA_EMUL_1G / 10;
	exp_offset[1] = BMA_EMUL_1G / 20;
	exp_offset[2] = -(int)BMA_EMUL_1G / 30;
	/* Disable rotation */
	ms.rot_standard_ref = NULL;

	/* Test set offset without rotation */
	zassert_equal(EC_SUCCESS, ms.drv->set_offset(&ms, exp_offset, temp),
		      NULL);
	get_emul_offset(emul, ret_offset);
	compare_int3v(exp_offset, ret_offset);

	/* Setup rotation and rotate input for set_offset function */
	ms.rot_standard_ref = &test_rotation;
	ret_offset[0] = exp_offset[0];
	ret_offset[1] = exp_offset[1];
	ret_offset[2] = exp_offset[2];
	rotate_int3v_by_test_rotation(ret_offset);

	/* Test get offset with rotation */
	zassert_equal(EC_SUCCESS, ms.drv->set_offset(&ms, ret_offset, temp),
		      NULL);
	get_emul_offset(emul, ret_offset);
	compare_int3v(exp_offset, ret_offset);
}
void test_suite_bma2x2(void)
{
	k_mutex_init(&sensor_mutex);

	ztest_test_suite(bma2x2,
			 ztest_user_unit_test(test_bma_get_offset),
			 ztest_user_unit_test(test_bma_set_offset));
	ztest_run_test_suite(bma2x2);
}
