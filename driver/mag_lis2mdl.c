/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * LIS2MDL magnetometer module for Chrome EC.
 * This driver supports LIS2MDL magnetometer in cascade with LSM6DSx (x stands
 * for L or M) accel/gyro module.
 */

#include "driver/mag_lis2mdl.h"
#include "driver/sensorhub_lsm6dsm.h"
#include "driver/stm_mems_common.h"
#include "task.h"

#define LIS2MDL_I2C_ADDR(__x)		(__x << 1)

#define LIS2MDL_ADDR0			LIS2MDL_I2C_ADDR(0x1e)
#define LIS2MDL_ADDR1			LIS2MDL_I2C_ADDR(0x1f)

#define LIS2MDL_WHO_AM_I_REG		0x4f
#define LIS2MDL_WHO_AM_I		0x40

#define LIS2MDL_CFG_REG_A_ADDR		0x60
#define LIS2MDL_SW_RESET		0x20
#define LIS2MDL_ODR_100HZ		0xc
#define LIS2MDL_CONT_MODE		0x0

#define LIS2MDL_STATUS_REG		0x67
#define LIS2MDL_OUT_REG			0x68

#define LIS2MDL_RANGE			4915
#define LIS2MDL_RESOLUTION		16

static int irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	if (s->addr != LIS2MDL_ADDR0) {
		/*
		 * Magnetometer in cascade mode. The main sensor should take
		 * care of interrupt situation.
		 */
		return EC_SUCCESS;
	}
	return EC_ERROR_UNIMPLEMENTED;
}

static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret = EC_ERROR_UNIMPLEMENTED;

	mutex_lock(s->mutex);
	if (s->addr != LIS2MDL_ADDR0)
		ret = sensorhub_set_ext_data_rate(s, rate, rnd);
	mutex_unlock(s->mutex);

	return ret;
}

static int set_range(const struct motion_sensor_t *s, int range, int rnd)
{
	struct stprivate_data *data = s->drv_data;

	/* Range is fixed to LIS2MDL_RANGE by hardware */
	data->base.range = LIS2MDL_RANGE;
	return EC_SUCCESS;
}

static int get_range(const struct motion_sensor_t *s)
{
	struct stprivate_data *data = s->drv_data;

	return data->base.range;
}

static int read(const struct motion_sensor_t *s, intv3_t v)
{
	int ret = EC_ERROR_UNIMPLEMENTED;

	mutex_lock(s->mutex);
	if (s->addr != LIS2MDL_ADDR0)
		ret = sensorhub_slv0_data_read(s, v);
	mutex_unlock(s->mutex);

	return ret;
}

static int init(const struct motion_sensor_t *s)
{
	int ret = EC_ERROR_UNIMPLEMENTED;
	struct stprivate_data *data = s->drv_data;

	mutex_lock(s->mutex);
	if (s->addr != LIS2MDL_ADDR0) {
		/* Magnetometer in cascade mode */
		ret = sensorhub_check_and_rst(s, LIS2MDL_ADDR0,
				LIS2MDL_WHO_AM_I_REG, LIS2MDL_WHO_AM_I,
				LIS2MDL_CFG_REG_A_ADDR, LIS2MDL_SW_RESET);
		if (ret != EC_SUCCESS)
			goto err_unlock;

		ret = sensorhub_config_ext_reg(s, LIS2MDL_ADDR0,
				LIS2MDL_CFG_REG_A_ADDR,
				LIS2MDL_ODR_100HZ | LIS2MDL_CONT_MODE);
		if (ret != EC_SUCCESS)
			goto err_unlock;

		ret = sensorhub_config_slv0_read(s, LIS2MDL_ADDR0,
					LIS2MDL_OUT_REG, OUT_XYZ_SIZE);
		if (ret != EC_SUCCESS)
			goto err_unlock;
	}

	/* Set default resolution to 16 bit */
	data->resol = LIS2MDL_RESOLUTION;
	/* Range is fixed to LIS2MDL_RANGE by hardware */
	data->base.range = LIS2MDL_RANGE;
	mutex_unlock(s->mutex);

	return sensor_init_done(s);

err_unlock:
	mutex_unlock(s->mutex);
	return ret;
}

const struct accelgyro_drv lis2mdl_drv = {
	.init = init,
	.read = read,
	.set_range = set_range,
	.get_range = get_range,
	.get_resolution = st_get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = st_get_data_rate,
	.set_offset = st_set_offset,
	.get_offset = st_get_offset,
	.irq_handler = irq_handler,
};
