/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * LIS2DH/LIS2DH12 accelerometer module for Chrome EC 3D digital accelerometer
 */

#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/accel_lis2dh.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "task.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

/* Internal data structure */
struct lis2dh_data lis2dh_a_data;

#ifdef CONFIG_ACCEL_FIFO
/* Define FIFO buffer to store data and process it */
static uint8_t lis2dh_buffer[CONFIG_ACCEL_FIFO];
#endif /* CONFIG_ACCEL_FIFO */

/**
 * Read single register
 */
static inline int raw_read8(const int port, const int addr, const int reg,
			    int *data_ptr)
{
	return i2c_read8(port, addr, reg, data_ptr);
}

/**
 * Write single register
 */
static inline int raw_write8(const int port, const int addr, const int reg,
			     int data)
{
	return i2c_write8(port, addr, reg, data);
}

/**
 * Read n bytes from accelerometer
 *
 * NOTE: SUB address MSB must be set for autoincrement in multi read
 */
static int raw_read_n(const int port, const int addr, const uint8_t reg,
		      uint8_t *data_ptr, const int len)
{
	int rv = -EC_ERROR_PARAM1;
	uint8_t reg_a = reg | I2C_AUTO_INC;

	i2c_lock(port, 1);
	rv = i2c_xfer(port, addr, &reg_a, 1, data_ptr, len, I2C_XFER_SINGLE);
	i2c_lock(port, 0);

	return rv;
}

 /**
 * write_data_with_mask - Write register with mask
 * @s: Motion sensor pointer
 * @reg: Device register
 * @mask: The mask to search
 * @data: Data pointer
 */
static int write_data_with_mask(const struct motion_sensor_t *s, int reg,
				uint8_t mask, uint8_t data)
{
	int err;
	int new_data = 0x00, old_data = 0x00;

	err = raw_read8(s->port, s->addr, reg, &old_data);
	if (err != EC_SUCCESS)
		return err;

	new_data = ((old_data & (~mask)) | ((data << __builtin_ctz(mask)) & mask));

	if (new_data == old_data)
		return EC_SUCCESS;

	return raw_write8(s->port, s->addr, reg, new_data);
}

#ifdef CONFIG_ACCEL_FIFO
/**
 * enable_fifo - Enable/Disable FIFO in LIS2DH
 * @s: Motion sensor pointer
 * @mode: enum lis2dh_fifo_modes
 * @en_dis: LIS2DH_EN_BIT/LIS2DH_DIS_BIT
 */
static int enable_fifo(const struct motion_sensor_t *s, int mode, int en_dis)
{
	int ret;

	ret = write_data_with_mask(s, LIS2DH_FIFO_CTRL_REG, LIS2DH_FIFO_MODE_MASK,
				   mode);
	if (ret != EC_SUCCESS)
		return ret;

	ret = write_data_with_mask(s, LIS2DH_CTRL5_ADDR, LIS2DH_FIFO_EN_MASK,
				   en_dis);

	return EC_SUCCESS;
}
#endif /* CONFIG_ACCEL_FIFO */

/**
 * set_range - set full scale range
 * @s: Motion sensor pointer
 * @range: Range
 * @rnd: Round up/down flag
 */
static int set_range(const struct motion_sensor_t *s,
		     int range, int rnd)
{
	int err, normalized_rate;
	struct lis2dh_data *data = s->drv_data;
	int val;

	val = LIS2DH_FS_TO_REG(range);
	normalized_rate = LIS2DH_FS_TO_NORMALIZE(range);

	if (rnd && (range < normalized_rate))
		val++;

	/* Adjust rounded values */
	if (val > LIS2DH_FS_16G_VAL) {
		val = LIS2DH_FS_16G_VAL;
		normalized_rate = 16;
	}

	if (val < LIS2DH_FS_2G_VAL) {
		val = LIS2DH_FS_2G_VAL;
		normalized_rate = 2;
	}
	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(s->mutex);
	err = write_data_with_mask(s, LIS2DH_CTRL4_ADDR, LIS2DH_FS_MASK, val);

	if (err == EC_SUCCESS)
		data->base.range = normalized_rate;

	mutex_unlock(s->mutex);
	return EC_SUCCESS;
}

static int get_range(const struct motion_sensor_t *s)
{
	struct lis2dh_data *data = s->drv_data;

	return data->base.range;
}

/* TODO: must support multiple resolution */
static int set_resolution(const struct motion_sensor_t *s, int res, int rnd)
{
	return EC_SUCCESS;
}

static int get_resolution(const struct motion_sensor_t *s)
{
	return LIS2DH_RESOLUTION;
}

static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret, normalized_rate;
	struct lis2dh_data *data = s->drv_data;
	uint8_t reg_val;

	mutex_lock(s->mutex);

#ifdef CONFIG_ACCEL_FIFO
	/* FIFO stop collecting events. Restart FIFO in Bypass mode */
	ret = enable_fifo(s, LIS2DH_FIFO_BYPASS_MODE, LIS2DH_DIS_BIT);
	if (ret != EC_SUCCESS)
		goto unlock_rate;
#endif /* CONFIG_ACCEL_FIFO */

	if (rate == 0) {
		/* Power Off device */
		ret = write_data_with_mask(s, LIS2DH_CTRL1_ADDR,
					   LIS2DH_ACC_ODR_MASK,
					   LIS2DH_ODR_POWER_OFF_VAL);
		goto unlock_rate;
	}

	reg_val = LIS2DH_ODR_TO_REG(rate);
	normalized_rate = LIS2DH_ODR_TO_NORMALIZE(rate);

	if (rnd && (normalized_rate < rate)) {
		reg_val++;
		normalized_rate = LIS2DH_REG_TO_NORMALIZE(reg_val);
	}

	/* Adjust rounded value */
	if (reg_val > LIS2DH_ODR_400HZ_VAL) {
		reg_val = LIS2DH_ODR_400HZ_VAL;
		normalized_rate = 400000;
	} else if (reg_val < LIS2DH_ODR_1HZ_VAL) {
		reg_val = LIS2DH_ODR_1HZ_VAL;
		normalized_rate = 1000;
	}

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done
	 */
	ret = write_data_with_mask(s, LIS2DH_CTRL1_ADDR, LIS2DH_ACC_ODR_MASK,
				   reg_val);
	if (ret == EC_SUCCESS)
		data->base.odr = normalized_rate;

#ifdef CONFIG_ACCEL_FIFO
	/* FIFO restart collecting events */
	ret = enable_fifo(s, LIS2DH_FIFO_STREAM_MODE, LIS2DH_EN_BIT);
#endif /* CONFIG_ACCEL_FIFO */

unlock_rate:
	mutex_unlock(s->mutex);
	return ret;
}

static int get_data_rate(const struct motion_sensor_t *s)
{
	struct lis2dh_data *data = s->drv_data;

	return data->base.odr;
}

static int set_offset(const struct motion_sensor_t *s,
		      const int16_t *offset, int16_t temp)
{
	struct lis2dh_data *data = s->drv_data;

	data->offset[X] = offset[X];
	data->offset[Y] = offset[Y];
	data->offset[Z] = offset[Z];
	return EC_SUCCESS;
}

static int get_offset(const struct motion_sensor_t *s,
		      int16_t *offset, int16_t *temp)
{
	struct lis2dh_data *data = s->drv_data;

	offset[X] = data->offset[X];
	offset[Y] = data->offset[Y];
	offset[Z] = data->offset[Z];
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

#ifdef CONFIG_ACCEL_FIFO
/*
 * Load data from internal sensor FIFO (deep 32 byte)
 */
static int load_fifo(struct motion_sensor_t *s)
{
	int ret, tmp, nsamples, i, j;
	struct ec_response_motion_sensor_data vect;
	struct lis2dh_data *drvdata = s->drv_data;
	int *axis = s->raw_xyz;

	/* Read samples number in status register */
	ret = raw_read8(s->port, s->addr, LIS2DH_FIFO_SRC_REG, &tmp);
	if (ret != EC_SUCCESS)
		return ret;

	/* FIFO empty */
	if (tmp & LIS2DH_FIFO_EMPTY_FLAG)
		return EC_SUCCESS;

	nsamples = (tmp & LIS2DH_FIFO_UNREAD_MASK) * LIS2DH_OUT_XYZ_SIZE;
	raw_read_n(s->port, s->addr, LIS2DH_OUT_X_L_ADDR, lis2dh_buffer, nsamples);
	if (ret != EC_SUCCESS)
		return ret;

	/* Read all FIFO samples */
	for (i = 0; i < nsamples; i += LIS2DH_OUT_XYZ_SIZE) {
		for (j = X; j <= Z; j++) {
			/* Adjust data to sensor Sensitivity and precision */
			axis[j] = ((int16_t)((lis2dh_buffer[ i + j * 2 + 1] << 8) |  
				lis2dh_buffer[i + j * 2]) >> 6);
			axis[j] = axis[j] * LIS2DH_FS_TO_GAIN(drvdata->base.range);

			/* Fill vector array */
			vect.data[j] = axis[j];
		}

		vect.flags = 0;
		vect.sensor_num = 0;
		rotate(axis, *s->rot_standard_ref, axis);
		motion_sense_fifo_add_unit(&vect, s, 3);
	}

	return EC_SUCCESS;
}
#endif  /* CONFIG_ACCEL_FIFO */

static int is_data_ready(const struct motion_sensor_t *s, int *ready)
{
	int ret, tmp;

	ret = raw_read8(s->port, s->addr, LIS2DH_STATUS_REG, &tmp);
	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RS Error]", s->name, s->type);
		return ret;
	}

	*ready = (LIS2DH_STS_XLDA_UP == (tmp & LIS2DH_STS_XLDA_UP));
	return EC_SUCCESS;
}

static int read(const struct motion_sensor_t *s, vector_3_t v)
{
	uint8_t raw[LIS2DH_OUT_XYZ_SIZE];
	int ret, i, tmp = 0;
	struct lis2dh_data *data = s->drv_data;

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

	/* Read output data bytes starting at LIS2DH_OUT_X_L_ADDR */
	ret = raw_read_n(s->port, s->addr, LIS2DH_OUT_X_L_ADDR, raw,
			 LIS2DH_OUT_XYZ_SIZE);
	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RD XYZ Error]",
			s->name, s->type);
		return ret;
	}

	for (i = X; i <= Z; i++) {
		/* 10 bit resolution */
		v[i] = ((int16_t)((raw[i * 2 + 1] << 8) | raw[i * 2]) >> 6);
		/* Multiply axis gain related to FS */
		v[i] = v[i] * LIS2DH_FS_TO_GAIN(data->base.range);
	}
	/* Apply rotation matrix */
	rotate(v, *s->rot_standard_ref, v);

	/* apply offset in the device coordinates */
	for (i = X; i <= Z; i++)
		v[i] += (data->offset[i] << 5) / data->base.range;

	return EC_SUCCESS;
}

static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp;

	ret = raw_read8(s->port, s->addr, LIS2DH_WHO_AM_I_REG, &tmp);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	if (tmp != LIS2DH_WHO_AM_I)
		return EC_ERROR_ACCESS_DENIED;

	mutex_lock(s->mutex);
	/* Device can be re-initialized after a reboot so any control
	 * register must be restored to it's default
	 */
	/* Enable all accel axes data and clear old settings */
	ret = raw_write8(s->port, s->addr, LIS2DH_CTRL1_ADDR,
			 LIS2DH_ENABLE_ALL_AXES);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	ret = raw_write8(s->port, s->addr, LIS2DH_CTRL2_ADDR,
			 LIS2DH_CTRL2_RESET_VAL);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	ret = raw_write8(s->port, s->addr, LIS2DH_CTRL3_ADDR,
			 LIS2DH_CTRL3_RESET_VAL);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	/* Enable BDU */
	ret = raw_write8(s->port, s->addr, LIS2DH_CTRL4_ADDR,
			 LIS2DH_BDU_MASK);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	ret = raw_write8(s->port, s->addr, LIS2DH_CTRL5_ADDR,
			 LIS2DH_CTRL5_RESET_VAL);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	ret = raw_write8(s->port, s->addr, LIS2DH_CTRL6_ADDR,
			 LIS2DH_CTRL6_RESET_VAL);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	mutex_unlock(s->mutex);

	/* Config initial Acc Range */
	ret = set_range(s, s->default_range, 0);
	if (ret != EC_SUCCESS)
		return ret;

	CPRINTF("[%T %s: MS Done Init type:0x%X range:%d]\n",
		s->name, s->type, get_range(s));
	return ret;

err_unlock:
	mutex_unlock(s->mutex);

	return EC_ERROR_UNKNOWN;
}

const struct accelgyro_drv lis2dh_drv = {
	.init = init,
	.read = read,
	.set_range = set_range,
	.get_range = get_range,
	.set_resolution = set_resolution,
	.get_resolution = get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = get_data_rate,
	.set_offset = set_offset,
	.get_offset = get_offset,
	.perform_calib = NULL,
#ifdef CONFIG_ACCEL_FIFO
	.load_fifo = load_fifo,
#endif /* CONFIG_ACCEL_FIFO */
};
