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

ZTEST_USER(bmi3xx, test_init)
{
	zassert_ok(acc->drv->init(acc));
}

static void bmi3xx_before(void *fixture)
{
	struct i2c_common_emul_data *common_data =
		emul_bmi3xx_get_i2c_common_data(emul);

	ARG_UNUSED(fixture);

	i2c_common_emul_set_read_fail_reg(common_data,
					  I2C_COMMON_EMUL_NO_FAIL_REG);
	bmi3xx_emul_reset(emul);
	memset(acc->raw_xyz, 0, sizeof(intv3_t));
	memset(gyr->raw_xyz, 0, sizeof(intv3_t));
	motion_sense_fifo_reset();
	acc->oversampling_ratio = 1;
	gyr->oversampling_ratio = 1;
}

ZTEST_SUITE(bmi3xx, drivers_predicate_post_main, NULL, bmi3xx_before, NULL,
	    NULL);
