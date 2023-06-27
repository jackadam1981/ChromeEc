/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "driver/accelgyro_bmi3xx.h"
#include "driver/accelgyro_bmi_common.h"
#include "emul/emul_bmi3xx.h"
#include "emul/emul_common_i2c.h"
#include "i2c.h"
#include "motion_sense_fifo.h"
#include "test/drivers/test_mocks.h"
#include "test/drivers/test_state.h"

#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define BMI3XX_NODE DT_NODELABEL(bmi3xx_emul)
#define ACC_SENSOR_ID SENSOR_ID(DT_NODELABEL(ms_bmi3xx_accel))
#define GYR_SENSOR_ID SENSOR_ID(DT_NODELABEL(ms_bmi3xx_gyro))

#define BMI_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(SENSOR_ID(DT_ALIAS(bmi3xx_int)))

static const struct emul *emul = EMUL_DT_GET(BMI3XX_NODE);
static struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
static struct motion_sensor_t *gyr = &motion_sensors[GYR_SENSOR_ID];

static bool check_sensor_enabled(enum motionsensor_type type)
{
	int reg = bmi3xx_emul_get_reg(emul, BMI3_REG_FIFO_CONF);

	printf("\t\t%s: 0x%x\n", __func__, reg);
	if (type == MOTIONSENSE_TYPE_ACCEL) {
		return reg & (BMI3_FIFO_ACC_EN << 8);
	} else if (type == MOTIONSENSE_TYPE_GYRO) {
		return reg & (BMI3_FIFO_GYR_EN << 8);
	}

	return false;
}

ZTEST_USER(bmi3xx, test_set_date_rate)
{
	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_ACCEL));
	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_GYRO));

	zassert_ok(acc->drv->set_data_rate(acc, 12500, 1));
	zassert_true(check_sensor_enabled(MOTIONSENSE_TYPE_ACCEL));
	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_GYRO));

	zassert_ok(gyr->drv->set_data_rate(gyr, 25000, 1));
	zassert_true(check_sensor_enabled(MOTIONSENSE_TYPE_ACCEL));
	zassert_true(check_sensor_enabled(MOTIONSENSE_TYPE_GYRO));

	zassert_ok(gyr->drv->set_data_rate(gyr, 0, 1));
	zassert_true(check_sensor_enabled(MOTIONSENSE_TYPE_ACCEL));
	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_GYRO));

	zassert_ok(acc->drv->set_data_rate(acc, 0, 1));
	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_ACCEL));
	zassert_false(check_sensor_enabled(MOTIONSENSE_TYPE_GYRO));

	zassert_ok(!(acc->drv->set_data_rate(acc, 1, 1)));
	zassert_ok(!(gyr->drv->set_data_rate(gyr, 1, 1)));
}

ZTEST_USER(bmi3xx, test_get_resolution)
{
	zassert_equal(acc->drv->get_resolution(acc), 16);
}

#define RANGE_SHIFT 4
#define RANGE_MSK 0x7
#define RANGE_2G 0x0
#define RANGE_4G 0x1
#define RANGE_8G 0x2
#define RANGE_16G 0x3
#define RANGE_125DPS 0x0
#define RANGE_250DPS 0x1
#define RANGE_500DPS 0x2
#define RANGE_1000DPS 0x3
#define RANGE_2000DPS 0x4

#define ODR_SHIFT 0
#define ODR_MSK 0xE
#define ODR_800 0xB
#define ODR_1600 0xC
ZTEST_USER(bmi3xx, test_set_range)
{
	int old_val, expect_val;
	struct ans {
		int rng;
		int rnd;
		int expect;
	} acci[] = {
		{ 1, 0, RANGE_2G },  { 5, 0, RANGE_4G },  { 5, 1, RANGE_8G },
		{ 16, 0, RANGE_16G }, { 16, 1, RANGE_16G },
	}, gyri[] = {
		{ 1500, 0, RANGE_1000DPS},
		{ 1500, 1, RANGE_2000DPS},
	};

	for (int i = 0; i < ARRAY_SIZE(acci); i++) {
		old_val = bmi3xx_emul_get_reg(emul, BMI3_REG_ACC_CONF);
		expect_val = (old_val & ~(RANGE_MSK << RANGE_SHIFT)) |
			     (acci[i].expect << RANGE_SHIFT);
		zassert_ok(acc->drv->set_range(acc, acci[i].rng, acci[i].rnd));
		zassert_equal(bmi3xx_emul_get_reg(emul, BMI3_REG_ACC_CONF),
			      expect_val);
	}

	for (int i = 0; i < ARRAY_SIZE(gyri); i++) {
		old_val = bmi3xx_emul_get_reg(emul, BMI3_REG_GYR_CONF);
		expect_val = (old_val & ~(RANGE_MSK << RANGE_SHIFT)) |
			     (gyri[i].expect << RANGE_SHIFT);
		zassert_ok(gyr->drv->set_range(gyr, gyri[i].rng, gyri[i].rnd));
		zassert_equal(bmi3xx_emul_get_reg(emul, BMI3_REG_GYR_CONF),
			      expect_val);
	}
}

ZTEST_USER(bmi3xx, test_init)
{
	/* test init okay */
	zassert_ok(acc->drv->init(acc));
	zassert_ok(gyr->drv->init(gyr));

	/* test invalid ID */
	bmi3xx_emul_set_reg(emul, BMI3_REG_CHIP_ID, 0x5566);
	zassert_equal(acc->drv->init(acc), EC_ERROR_HW_INTERNAL);
}

ZTEST_USER(bmi3xx, test_read_temp)
{
	int temp;

	zassert_ok(acc->drv->init(acc));

	/* function unimplemented yet */
	zassert_equal(EC_ERROR_UNIMPLEMENTED, acc->drv->read_temp(acc, &temp));
}

static void bmi3xx_before(void *fixture)
{
	struct i2c_common_emul_data *common_data =
		emul_bmi3xx_get_i2c_common_data(emul);

	ARG_UNUSED(fixture);

	i2c_common_emul_set_read_fail_reg(common_data,
					  I2C_COMMON_EMUL_NO_FAIL_REG);
	bmi323_emul_reset(emul);
	memset(acc->raw_xyz, 0, sizeof(intv3_t));
	memset(gyr->raw_xyz, 0, sizeof(intv3_t));
	motion_sense_fifo_reset();
	acc->oversampling_ratio = 1;
	gyr->oversampling_ratio = 1;
}

ZTEST_SUITE(bmi3xx, drivers_predicate_post_main, NULL, bmi3xx_before, NULL,
	    NULL);
