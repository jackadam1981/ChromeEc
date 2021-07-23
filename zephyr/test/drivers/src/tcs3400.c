/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>

#include "common.h"
#include "i2c.h"
#include "emul/emul_tcs3400.h"

#include "motion_sense.h"
#include "motion_sense_fifo.h"
#include "driver/als_tcs3400.h"

#define TCS_ORD			DT_DEP_ORD(DT_NODELABEL(tcs_emul))
#define TCS_CLR_SENSOR_ID	SENSOR_ID(DT_NODELABEL(tcs3400_clear))
#define TCS_RGB_SENSOR_ID	SENSOR_ID(DT_NODELABEL(tcs3400_rgb))
#define TCS_INT_EVENT		\
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(SENSOR_ID(DT_ALIAS(tcs3400_int)))

/** How accurate comparision of rgb sensors should be */
#define V_EPS		400

static void test_tcs_init(void)
{
	struct motion_sensor_t *ms, *ms_rgb;
	struct i2c_emul *emul;

	emul = tcs_emul_get(TCS_ORD);
	ms = &motion_sensors[TCS_CLR_SENSOR_ID];
	ms_rgb = &motion_sensors[TCS_RGB_SENSOR_ID];

	/* RGB sensor initialization is always successful */
	zassert_equal(EC_SUCCESS, ms_rgb->drv->init(ms_rgb), NULL);

	/* Fail init on communication errors */
	tcs_emul_set_read_fail_reg(emul, TCS_EMUL_FAIL_ALL_REG);
	zassert_equal(-EIO, ms->drv->init(ms), NULL);
	tcs_emul_set_read_fail_reg(emul, TCS_EMUL_NO_FAIL_REG);

	/* Fail on bad ID */
	tcs_emul_set_reg(emul, TCS_I2C_ID, 0);
	zassert_equal(EC_ERROR_ACCESS_DENIED, ms->drv->init(ms), NULL);
	/* Restore ID */
	tcs_emul_set_reg(emul, TCS_I2C_ID, DT_ENUM_TOKEN(DT_NODELABEL(tcs_emul),
							 device_id));

	/* Test successful init. ATIME and AGAIN should be changed on init */
	zassert_equal(EC_SUCCESS, ms->drv->init(ms), NULL);
	zassert_equal(TCS_DEFAULT_ATIME,
		      tcs_emul_get_reg(emul, TCS_I2C_ATIME), NULL);
	zassert_equal(TCS_DEFAULT_AGAIN,
		      tcs_emul_get_reg(emul, TCS_I2C_CONTROL), NULL);
}

static void test_tcs_read(void)
{
	struct motion_sensor_t *ms;
	struct i2c_emul *emul;
	uint8_t enable;
	intv3_t v;

	emul = tcs_emul_get(TCS_ORD);
	ms = &motion_sensors[TCS_CLR_SENSOR_ID];

	/* Test error on writing registers */
	tcs_emul_set_write_fail_reg(emul, TCS_I2C_ATIME);
	zassert_equal(-EIO, ms->drv->read(ms, v), NULL);
	tcs_emul_set_write_fail_reg(emul, TCS_I2C_CONTROL);
	zassert_equal(-EIO, ms->drv->read(ms, v), NULL);
	tcs_emul_set_write_fail_reg(emul, TCS_I2C_ENABLE);
	zassert_equal(-EIO, ms->drv->read(ms, v), NULL);
	tcs_emul_set_write_fail_reg(emul, TCS_EMUL_NO_FAIL_REG);

	/* Test starting read with calibration */
	tcs_emul_set_reg(emul, TCS_I2C_ATIME, 0);
	tcs_emul_set_reg(emul, TCS_I2C_CONTROL, 0);
	tcs_emul_set_reg(emul, TCS_I2C_ENABLE, 0);
	zassert_equal(EC_SUCCESS, ms->drv->perform_calib(ms, 1), NULL);
	zassert_equal(EC_RES_IN_PROGRESS, ms->drv->read(ms, v), NULL);
	zassert_equal(TCS_CALIBRATION_ATIME,
		      tcs_emul_get_reg(emul, TCS_I2C_ATIME), NULL);
	zassert_equal(TCS_CALIBRATION_AGAIN,
		      tcs_emul_get_reg(emul, TCS_I2C_CONTROL), NULL);
	enable = tcs_emul_get_reg(emul, TCS_I2C_ENABLE);
	zassert_true(enable & TCS_I2C_ENABLE_POWER_ON, NULL);
	zassert_true(enable & TCS_I2C_ENABLE_ADC_ENABLE, NULL);
	zassert_true(enable & TCS_I2C_ENABLE_INT_ENABLE, NULL);

	/* Test starting read without calibration */
	tcs_emul_set_reg(emul, TCS_I2C_ATIME, 0);
	tcs_emul_set_reg(emul, TCS_I2C_CONTROL, 0);
	tcs_emul_set_reg(emul, TCS_I2C_ENABLE, 0);
	zassert_equal(EC_SUCCESS, ms->drv->perform_calib(ms, 0), NULL);
	zassert_equal(EC_RES_IN_PROGRESS, ms->drv->read(ms, v), NULL);
//	zassert_equal(TCS_DEFAULT_ATIME,
//		      tcs_emul_get_reg(emul, TCS_I2C_ATIME), NULL);
//	zassert_equal(TCS_DEFAULT_AGAIN,
//		      tcs_emul_get_reg(emul, TCS_I2C_CONTROL), NULL);
	enable = tcs_emul_get_reg(emul, TCS_I2C_ENABLE);
	zassert_true(enable & TCS_I2C_ENABLE_POWER_ON, NULL);
	zassert_true(enable & TCS_I2C_ENABLE_ADC_ENABLE, NULL);
	zassert_true(enable & TCS_I2C_ENABLE_INT_ENABLE, NULL);
}

static void check_fifo_empty_f(struct motion_sensor_t *ms,
			       struct motion_sensor_t *ms_rgb, int line)
{
	struct ec_response_motion_sensor_data vector;
	uint16_t size;

	/* Read all data committed to FIFO */
	while (motion_sense_fifo_read(sizeof(vector), 1, &vector, &size)) {
		/* Ignore timestamp frames */
		if (vector.flags == MOTIONSENSE_SENSOR_FLAG_TIMESTAMP) {
			continue;
		}

		if (ms - motion_sensors == vector.sensor_num) {
			zassert_unreachable("Unexpected frame for clear sensor");
		}

		if (ms_rgb - motion_sensors == vector.sensor_num) {
			zassert_unreachable("Unexpected frame for rgb sensor");
		}
	}
}
#define check_fifo_empty(ms, ms_rgb)		\
	check_fifo_empty_f(ms, ms_rgb, __LINE__)

static void test_tcs_irq_handler_fail(void)
{
	struct motion_sensor_t *ms, *ms_rgb;
	struct i2c_emul *emul;
	uint32_t event;

	emul = tcs_emul_get(TCS_ORD);
	ms = &motion_sensors[TCS_CLR_SENSOR_ID];
	ms_rgb = &motion_sensors[TCS_RGB_SENSOR_ID];

	/* Fail on wrong event */
	event = 0x1234 & ~TCS_INT_EVENT;
	zassert_equal(EC_ERROR_NOT_HANDLED, ms->drv->irq_handler(ms, &event),
		      NULL);
	check_fifo_empty(ms, ms_rgb);

	event = TCS_INT_EVENT;
	/* Test error on reading status */
	tcs_emul_set_read_fail_reg(emul, TCS_I2C_STATUS);
	zassert_equal(-EIO, ms->drv->irq_handler(ms, &event), NULL);
	tcs_emul_set_read_fail_reg(emul, TCS_EMUL_NO_FAIL_REG);
	check_fifo_empty(ms, ms_rgb);

	/* Test fail on changing device power state */
	tcs_emul_set_write_fail_reg(emul, TCS_I2C_ENABLE);
	zassert_equal(-EIO, ms->drv->irq_handler(ms, &event), NULL);
	tcs_emul_set_write_fail_reg(emul, TCS_EMUL_NO_FAIL_REG);
	check_fifo_empty(ms, ms_rgb);

	/* Test that no data is commited when status is 0 */
	tcs_emul_set_reg(emul, TCS_I2C_STATUS, 0);
	zassert_equal(EC_SUCCESS, ms->drv->irq_handler(ms, &event), NULL);
	check_fifo_empty(ms, ms_rgb);
}

/**
 * Run irq handler on accelerometer sensor and check if committed data in FIFO
 * match what was set in FIFO frames in emulator.
 */
static void check_fifo_f(struct motion_sensor_t *ms,
			 struct motion_sensor_t *ms_rgb,
			 int *exp_v, int eps, int line)
{
	struct ec_response_motion_sensor_data vector;
	uint16_t size;
	int ret_v[4] = {-1, -1, -1, -1};
	int i;

	/* Read all data committed to FIFO */
	while (motion_sense_fifo_read(sizeof(vector), 1, &vector, &size)) {
		/* Ignore timestamp frames */
		if (vector.flags == MOTIONSENSE_SENSOR_FLAG_TIMESTAMP) {
			continue;
		}

		/* Get clear frame */
		if (ms - motion_sensors == vector.sensor_num) {
			if (ret_v[0] != -1) {
				zassert_unreachable(
					"More than one frame for clear sensor, line %d",
					line);
			}
			ret_v[0] = vector.udata[0];
		}

		/* Get rgb frame */
		if (ms_rgb - motion_sensors == vector.sensor_num) {
			if (ret_v[1] != -1) {
				zassert_unreachable(
					"More than one frame for rgb sensor, line %d",
					line);
			}
			ret_v[1] = vector.udata[0];
			ret_v[2] = vector.udata[1];
			ret_v[3] = vector.udata[2];
		}
	}

	if (ret_v[0] == -1) {
		return;
		zassert_unreachable("No frame for clear sensor, line %d", line);
	}

	if (ret_v[1] == -1) {
		return;
		zassert_unreachable("No frame for rgb sensor, line %d", line);
	}

	//for (i = 0; i < 4; i++) {
	//	zassert_within(exp_v[i], ret_v[i], eps,

	printf("Expected [%d; %d; %d; %d], got [%d; %d; %d; %d]; line: %d",
			exp_v[0], exp_v[1], exp_v[2], exp_v[3],
			ret_v[0], ret_v[1], ret_v[2], ret_v[3], line);
//	}
	printf("\n");
}
#define check_fifo(ms, ms_rgb, exp_v, eps)		\
	check_fifo_f(ms, ms_rgb, exp_v, eps, __LINE__)

static void test_tcs_read_calibration(void)
{
	struct motion_sensor_t *ms, *ms_rgb;
	struct i2c_emul *emul;
	uint32_t event = TCS_INT_EVENT;
	int emul_v[4];
	int exp_v[4];
	int ret;
	intv3_t v;

	emul = tcs_emul_get(TCS_ORD);
	ms = &motion_sensors[TCS_CLR_SENSOR_ID];
	ms_rgb = &motion_sensors[TCS_RGB_SENSOR_ID];

	/* Need to be set to collect all data in FIFO */
	ms->oversampling_ratio = 1;
	ms_rgb->oversampling_ratio = 1;
	/* Enable calibration mode */
	zassert_equal(EC_SUCCESS, ms->drv->perform_calib(ms, 1), NULL);
	/* Setup AGAIN and ATIME for calibration */
	zassert_equal(EC_RES_IN_PROGRESS, ms->drv->read(ms, v), NULL);

	/* Test data that are in calibration range */
	exp_v[0] = 12;
	exp_v[1] = 123;
	exp_v[2] = 1234;
	exp_v[3] = 12345;
	/* Emulator value is with gain 64, while expected value is with gain 16 */
	emul_v[0] = exp_v[0] * 64 / 16;
	emul_v[1] = exp_v[1] * 64 / 16;
	emul_v[2] = exp_v[2] * 64 / 16;
	emul_v[3] = exp_v[3] * 64 / 16;
	tcs_emul_set_val(emul, TCS_EMUL_C, emul_v[0]);
	tcs_emul_set_val(emul, TCS_EMUL_R, emul_v[1]);
	tcs_emul_set_val(emul, TCS_EMUL_G, emul_v[2]);
	tcs_emul_set_val(emul, TCS_EMUL_B, emul_v[3]);
	/* Set status to show valid data */
	tcs_emul_set_reg(emul, TCS_I2C_STATUS, TCS_I2C_STATUS_RGBC_VALID);

	zassert_equal(EC_SUCCESS, ms->drv->irq_handler(ms, &event), NULL);
	/* In calibration mode check for exact match */
	check_fifo(ms, ms_rgb, exp_v, 1);

	/* Test data that are outside of calibration range */
	uint16_t m = UINT16_MAX;
	exp_v[0] = 0;
	exp_v[1] = UINT16_MAX;
	exp_v[2] = UINT16_MAX;
	exp_v[3] = 213;
	/* Emulator value is with gain 64, while expected value is with gain 16 */
	emul_v[0] = 0;
	emul_v[1] = exp_v[1] * 64 / 16;
	emul_v[2] = (UINT16_MAX + 23) * 64 / 16;
	emul_v[3] = exp_v[3] * 64 / 16;
	tcs_emul_set_val(emul, TCS_EMUL_C, emul_v[0]);
	tcs_emul_set_val(emul, TCS_EMUL_R, emul_v[1]);
	tcs_emul_set_val(emul, TCS_EMUL_G, emul_v[2]);
	tcs_emul_set_val(emul, TCS_EMUL_B, emul_v[3]);
	/* Set status to show valid data */
	tcs_emul_set_reg(emul, TCS_I2C_STATUS, TCS_I2C_STATUS_RGBC_VALID);

	zassert_equal(EC_SUCCESS, ms->drv->irq_handler(ms, &event), NULL);
	/* In calibration mode check for exact match */
	check_fifo(ms, ms_rgb, exp_v, 1);
}

static void set_emul_val_from_exp(int *exp_v, struct i2c_emul *emul)
{
	int emul_v[4];
	int ir;

	/* We use exp_v[0] as IR value */
	ir = exp_v[0];
	/* Driver will return lux value as calculated blue light value */
	exp_v[0] = exp_v[2];

	/*
	 * Driver takes care of different ATIME and AGAIN value, so expected
	 * value is always normalized to ATIME 256 and AGAIN 16. Convert it
	 * to internal emulator value (ATIME 256, AGAIN 64) and add expected IR
	 * value. Clear light is the sum of rgb light without IR component.
	 */
	emul_v[1] = (exp_v[1] + ir) * 64 / 16;
	emul_v[2] = (exp_v[2] + ir) * 64 / 16;
	emul_v[3] = (exp_v[3] + ir) * 64 / 16;
	emul_v[0] = (exp_v[1] + exp_v[2] + exp_v[3]) * 64 / 16;
	tcs_emul_set_val(emul, TCS_EMUL_C, emul_v[0]);
	tcs_emul_set_val(emul, TCS_EMUL_R, emul_v[1]);
	tcs_emul_set_val(emul, TCS_EMUL_G, emul_v[2]);
	tcs_emul_set_val(emul, TCS_EMUL_B, emul_v[3]);
}

static void test_tcs_read_xyz(void)
{
	struct motion_sensor_t *ms, *ms_rgb;
	struct i2c_emul *emul;
	uint32_t event = TCS_INT_EVENT;
	int emul_v[4];
	int exp_v[4];
	int ret, i;
	intv3_t v;

	emul = tcs_emul_get(TCS_ORD);
	ms = &motion_sensors[TCS_CLR_SENSOR_ID];
	ms_rgb = &motion_sensors[TCS_RGB_SENSOR_ID];

	/* Need to be set to collect all data in FIFO */
	ms->oversampling_ratio = 1;
	ms_rgb->oversampling_ratio = 1;
	/* Disable calibration mode */
	zassert_equal(EC_SUCCESS, ms->drv->perform_calib(ms, 0), NULL);
	/* Setup AGAIN and ATIME for calibration */
	zassert_equal(EC_RES_IN_PROGRESS, ms->drv->read(ms, v), NULL);

	/* Test different data in supported range */
	exp_v[0] = 200;
	exp_v[1] = 1110;
	exp_v[2] = 870;
	exp_v[3] = 850;
	set_emul_val_from_exp(exp_v, emul);
	/* Set status to show valid data */
	tcs_emul_set_reg(emul, TCS_I2C_STATUS, TCS_I2C_STATUS_RGBC_VALID);
	zassert_equal(EC_SUCCESS, ms->drv->irq_handler(ms, &event), NULL);
	check_fifo(ms, ms_rgb, exp_v, V_EPS);

	exp_v[0] = 300;
	exp_v[1] = 1110;
	exp_v[2] = 10000;
	exp_v[3] = 8500;
	set_emul_val_from_exp(exp_v, emul);
	/* Set status to show valid data */
	tcs_emul_set_reg(emul, TCS_I2C_STATUS, TCS_I2C_STATUS_RGBC_VALID);
	zassert_equal(EC_SUCCESS, ms->drv->irq_handler(ms, &event), NULL);
	check_fifo(ms, ms_rgb, exp_v, V_EPS);

	exp_v[0] = 600;
	exp_v[1] = 50000;
	exp_v[2] = 40000;
	exp_v[3] = 30000;
	set_emul_val_from_exp(exp_v, emul);
	/* Set status to show valid data */
	tcs_emul_set_reg(emul, TCS_I2C_STATUS, TCS_I2C_STATUS_RGBC_VALID);
	zassert_equal(EC_SUCCESS, ms->drv->irq_handler(ms, &event), NULL);
	check_fifo(ms, ms_rgb, exp_v, V_EPS);
	tcs_emul_set_reg(emul, TCS_I2C_STATUS, TCS_I2C_STATUS_RGBC_VALID);
	zassert_equal(EC_SUCCESS, ms->drv->irq_handler(ms, &event), NULL);
	check_fifo(ms, ms_rgb, exp_v, V_EPS);

	/* Test data that are outside of supported range*/
	exp_v[0] = 3000;
	exp_v[1] = UINT16_MAX + 5000;
	exp_v[2] = UINT16_MAX * 3;
	exp_v[3] = 0;
	set_emul_val_from_exp(exp_v, emul);
	/* Set status to show valid data */
	tcs_emul_set_reg(emul, TCS_I2C_STATUS, TCS_I2C_STATUS_RGBC_VALID);
	zassert_equal(EC_SUCCESS, ms->drv->irq_handler(ms, &event), NULL);
	//check_fifo(ms, ms_rgb, exp_v, V_EPS);
	/*
	 * If saturation value is exceeded on any rgb sensor, than data
	 * shouldn't be commited to FIFO.
	 */
	check_fifo_empty(ms, ms_rgb);
}

static void test_tcs_set_range(void)
{
	struct motion_sensor_t *ms, *ms_rgb;
	struct i2c_emul *emul;

	emul = tcs_emul_get(TCS_ORD);
	ms = &motion_sensors[TCS_CLR_SENSOR_ID];
	ms_rgb = &motion_sensors[TCS_RGB_SENSOR_ID];

	/* RGB sensor doesn't set anything */
	zassert_equal(EC_SUCCESS, ms_rgb->drv->set_range(ms_rgb, 1, 0), NULL);

	/* Clear sensor doesn't change anything on device to set range */
	zassert_equal(EC_SUCCESS, ms->drv->set_range(ms, 0x12300, 1), NULL);
	zassert_equal(0x12300, ms->current_range, NULL);

	zassert_equal(EC_SUCCESS, ms->drv->set_range(ms, 0x10000, 0), NULL);
	zassert_equal(0x10000, ms->current_range, NULL);
}

void test_suite_tcs3400(void)
{
	ztest_test_suite(tcs3400,
			 ztest_user_unit_test(test_tcs_init),
			 ztest_user_unit_test(test_tcs_read),
			 ztest_user_unit_test(test_tcs_irq_handler_fail),
			 ztest_user_unit_test(test_tcs_read_calibration),
			 ztest_user_unit_test(test_tcs_read_xyz),
			 ztest_user_unit_test(test_tcs_set_range));
	ztest_run_test_suite(tcs3400);
}
