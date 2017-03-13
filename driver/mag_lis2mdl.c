/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * LIS2MDL accelerometer module for Chrome EC 3D digital accelerometer
 * This driver manage Mag as stand alone device, not cascaded with other
 * device (on I2C master interface)
 */

#include "common.h"
#include "hooks.h"
#include "math_util.h"
#include "task.h"
#include "util.h"
#include "driver/stm_mems_common.h"

struct stprivate_data lis2mdl_m_data;

#if defined(CONFIG_MAG_LSM6DSM_LIS2MDL) && defined(CONFIG_LSM6DSM_PASSTHROUGH)

/**
 * Cascade mag LIS2MDL + acc_gyro LSM6DSM/L
 * Use internal acc_gyro FIFO
 * Use acc_gyro Master I2C interface
 * Accelerometer event driven
 * LSM6DSM/L I2C Passthrough mode. In this case use directly MAG registers
 * @s - Motion Sensor struct
 */
int init_lis2mdl(const struct motion_sensor_t *s)
{
	int ret = 0, tmp;
	struct stprivate_data *data = s->drv_data;

	ret = raw_read8(s->port, s->addr, LIS2MDL_WHO_AM_I_REG, &tmp);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	if (tmp != LIS2MDL_WHOAMI_VAL)
		return EC_ERROR_ACCESS_DENIED;

	/* Set continuous MODE */
	ret = st_write_data_with_mask(s, LIS2MDL_CFG_REG_A, LIS2MDL_MODE_MASK,
				      LIS2MDL_CONT_MODE);
	if (ret != EC_SUCCESS)
		return ret;

	/* Enable BDU */
	ret = st_write_data_with_mask(s, LIS2MDL_CFG_REG_C, LIS2MDL_BDU_MASK,
				      LIS2MDL_EN_BIT);
	if (ret != EC_SUCCESS)
		return ret;

	/* Set sensor resolution in bit */
	data->resol = LIS2MDL_RESOLUTION;

	return EC_SUCCESS;
}

#else /* CONFIG_MAG_LSM6DSM_LIS2MDL && CONFIG_LSM6DSM_PASSTHROUGH */

/**
 * In case of stand alone magnetometer LIS2MDL has not internal FIFO support
 * so just poll data
 */

/**
 * set_range - set full scale range
 * @s: Motion sensor pointer
 * @range: Range
 * @rnd: Round up/down flag
 *
 * This sensor has only one range +/- 49.152 Gauss
 */
static int set_range(const struct motion_sensor_t *s, int range, int rnd)
{
	return EC_SUCCESS;
}

/**
 * get_range - Get full scale range
 * @s: Motion sensor pointer
 *
 * See set_range
 */
static int get_range(const struct motion_sensor_t *s)
{
	struct stprivate_data *data = s->drv_data;

	return data->base.range;
}

/**
 * set_data_rate - Set out data rate
 * @s: Motion sensor pointer
 * @rate: Rate in mHz
 * @rnd: Round up/down flag
 */
static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret = EC_SUCCESS;
	struct stprivate_data *data = s->drv_data;

	if (MOTIONSENSE_TYPE_MAG != s->type)
		return EC_SUCCESS;

	mutex_lock(s->mutex);
	if (rate == 0) {
		/* Set in idle */
		ret = st_write_data_with_mask(s, LIS2MDL_CFG_REG_A, 
					      LIS2MDL_MAG_MODE_MSK,
					      LIS2MDL_MD_IDLE1_MODE);
		mutex_unlock(s->mutex);
		if (ret != EC_SUCCESS)
			goto m_unlock;

		/* power off mag */
		data->base.odr = 0;
	} else {
		int normalized_rate = LIS2MDL_ODR_TO_NORMALIZE(rate);

		if (rnd && rate < normalized_rate)
			normalized_rate = LSM6DSM_ODR_TO_NORMALIZE(
						normalized_rate + 1);
			
		/* adjust rounded value */
		if (normalized_rate > LIS2MDL_ODR100_HZ) {
			normalized_rate = LIS2MDL_ODR100_HZ;
		} else if (normalized_rate < LIS2MDL_ODR10_HZ) {
			normalized_rate = LIS2MDL_ODR10_HZ;
		}

		/* set new ODR and reenable coninuos mode */
		ret = st_write_data_with_mask(s, LIS2MDL_CFG_REG_A, 
				LIS2MDL_ODR_MASK | LIS2MDL_MAG_MODE_MSK,
				normalized_rate | LIS2MDL_MD_CONTINUOS_MODE);
		if (ret == EC_SUCCESS) 
			data->base.odr = normalized_rate;
	}

m_unlock:
	mutex_unlock(s->mutex);

	return ret;
}

/**
 * is_data_ready - Check data ready to be read on mag
 * @s: Motion sensor pointer
 * @ready: Ready flag
 */
static int is_data_ready(const struct motion_sensor_t *s, int *ready)
{
	int ret, tmp;

	ret = raw_read8(s->port, s->addr, LIS2MDL_STATUS_REG, &tmp);
	if (ret != EC_SUCCESS)
		return ret;

	*ready = (LIS2MDL_STS_MDA_UP == (tmp & LIS2MDL_STS_MDA_UP));
	return EC_SUCCESS;
}

/**
 * read - Single data read from mag (polling data)
 * @s: Motion sensor pointer
 * @v: Output data normalized
 */
static int read(const struct motion_sensor_t *s, vector_3_t v)
{
	uint8_t raw[LIS2MDL_OUT_REG_SIZE];
	int ret, i, tmp = 0;
	struct stprivate_data *data = s->drv_data;

	ret = is_data_ready(s, &tmp);
	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * If sensor data is not ready, return the previous read data.
	 * Note: return success so that motion senor task can read again
	 * to get the latest updated sensor data quickly.
	 */
	if (!tmp) {
		if (v != s->raw_xyz)
			memcpy(v, s->raw_xyz, sizeof(s->raw_xyz));
		return EC_SUCCESS;
	}

	/* Read sensor output data */
	ret = st_raw_read_n(s->port, s->addr, LIS2MDL_OUT_REG, raw,
			    LIS2MDL_OUT_REG_SIZE);
	if (ret != EC_SUCCESS)
		return ret;

	/* Transform from LSB to real data with rotation and gain */
	st_normalize(s, v, raw);

	/* apply offset in the device coordinates */
	for (i = X; i <= Z; i++)
		v[i] += (data->offset[i] << 5) / LIS2MDL_RANGE;

	return EC_SUCCESS;
}

/**
 * init - Init mag in case of stand alone solution
 * @s: Motion sensor pointer
 */
static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp, timeout = 0, status;
	struct stprivate_data *data = s->drv_data;

	ret = raw_read8(s->port, s->addr, LIS2MDL_WHO_AM_I_REG, &tmp);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	if (tmp != LIS2MDL_WHOAMI_VAL)
		return EC_ERROR_ACCESS_DENIED;

	/*
	 * This sensor can be powered through an EC reboot, so the state of
	 * the sensor is unknown here. Initiate software reset to restore
	 * sensor to default.
	 */
	mutex_lock(s->mutex);

	/* Reset component and set mode to continuous and BDU */
	ret = raw_write8(s->port, s->addr, LIS2MDL_CFG_REG_A, LIS2MDL_SOFT_RST);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	/* Check End of Reset (should be less than 11 ms in any case) */
	do {
		if (timeout > 15) {
			ret = EC_RES_TIMEOUT;
			goto err_unlock;
		}

		msleep(5);
		timeout += 5;
		ret = raw_read8(s->port, s->addr, LIS2MDL_CFG_REG_A, &status);
		if (ret != EC_SUCCESS)
			continue;
	} while ((status & LIS2MDL_REBOOT) != 0);

	/* Set continuous MODE */
	ret = st_write_data_with_mask(s, LIS2MDL_CFG_REG_A, LIS2MDL_MODE_MASK,
				      LIS2MDL_CONT_MODE);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	/* Enable BDU */
	ret = st_write_data_with_mask(s, LIS2MDL_CFG_REG_C, LIS2MDL_BDU_MASK,
				      LIS2MDL_EN_BIT);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	/* Set sensor resolution in bit */
	data->resol = LIS2MDL_RESOLUTION;
	/* Set sensor range (fixed) */
	data->base.range = LIS2MDL_RANGE;
	mutex_unlock(s->mutex);

	CPRINTF("[%T %s: MS Done Init type:0x%X range:%d]\n",
		s->name, s->type, get_range(s));
	return ret;

err_unlock:
	mutex_unlock(s->mutex);

	return EC_ERROR_UNKNOWN;
}

const struct accelgyro_drv lis2mdl_drv = {
	.init = init,
	.read = read,
	.set_range = set_range,
	.get_range = get_range,
	.set_resolution = st_set_resolution,
	.get_resolution = st_get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = st_get_data_rate,
	.set_offset = st_set_offset,
	.get_offset = st_get_offset,
	.perform_calib = NULL,
};
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL && CONFIG_LSM6DSM_PASSTHROUGH */
