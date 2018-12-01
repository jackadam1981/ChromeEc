/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * LIS2MDL magnetometer module for Chrome EC.
 * This driver supports LIS2MDL magnetometer in cascade with LSM6DSx (x stands
 * for L or M) accel/gyro module.
 */

#include "common.h"
#include "driver/mag_lis2mdl.h"
#include "driver/sensorhub_lsm6dsm.h"
#include "driver/stm_mems_common.h"
#include "task.h"

#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
#ifndef CONFIG_SENSORHUB_LSM6DSM
#error "Need Sensor Hub LSM6DSM support"
#endif
#include "driver/accelgyro_lsm6dsm.h"

int lis2mdl_thru_lsm6dsm_read(const struct motion_sensor_t *s, uint8_t *v)
{
	int ret;
	/*
	 * This is mostly for debugging, read happens through LSM6DSM/BMI160
	 * FIFO.
	 */
	mutex_lock(s->mutex);
	ret = sensorhub_slv0_data_read(LSM6DSM_MAIN_SENSOR(s), v);
	mutex_unlock(s->mutex);
	return ret;
}

int lis2mdl_thru_lsm6dsm_init(const struct motion_sensor_t *s)
{
	int ret = EC_ERROR_UNIMPLEMENTED;
	struct stprivate_data *data = s->drv_data;

	mutex_lock(s->mutex);
	/* Magnetometer in cascade mode */
	ret = sensorhub_check_and_rst(
			LSM6DSM_MAIN_SENSOR(s),
			CONFIG_ACCELGYRO_SEC_ADDR,
			LIS2MDL_WHO_AM_I_REG, LIS2MDL_WHO_AM_I,
			LIS2MDL_CFG_REG_A_ADDR, LIS2MDL_SW_RESET);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	ret = sensorhub_config_ext_reg(
			LSM6DSM_MAIN_SENSOR(s),
			CONFIG_ACCELGYRO_SEC_ADDR,
			LIS2MDL_CFG_REG_A_ADDR,
			LIS2MDL_ODR_100HZ | LIS2MDL_CONT_MODE);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	ret = sensorhub_config_slv0_read(
			LSM6DSM_MAIN_SENSOR(s),
			CONFIG_ACCELGYRO_SEC_ADDR,
			LIS2MDL_OUT_REG, OUT_XYZ_SIZE);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	/* Set default resolution to 16 bit */
	data->resol = LIS2MDL_RESOLUTION;
	/* Range is fixed to LIS2MDL_RANGE by hardware */
	data->base.range = LIS2MDL_RANGE;
err_unlock:
	mutex_unlock(s->mutex);
	return ret;
}

#endif  /* CONFIG_MAG_LSM6DSM_LIS2MDL */
