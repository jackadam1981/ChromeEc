/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI CM32181E light sensor driver
 */

#include "common.h"
#include "driver/als_cm32181e.h"
#include "i2c.h"
#include "accelgyro.h"
#include "math_util.h"

/**
 *  Read register from CM32181E light sensor.
 */
static int cm32181e_i2c_read(const int port, const int addr, const int reg,
			    int *data_ptr)
{
	int ret;

	ret = i2c_read16(port, addr, reg, data_ptr);
	return ret;
}

/**
 *  Write register to CM32181E light sensor.
 */
static int cm32181e_i2c_write(const int port, const int addr, const int reg,
			     int data)
{
	return i2c_write16(port, addr, reg, data);
}

/**
 * Read CM32181E light sensor data.
 */
int cm32181e_read_lux(const struct motion_sensor_t *s, vector_3_t v)
{
	struct cm32181e_drv_data_t *drv_data = CM32181E_GET_DATA(s);
	int ret;
	int data;

	ret = cm32181e_i2c_read(s->port, s->addr, CM32181E_REG_RESULT, &data);
	if (ret)
		return ret;

	/*
	 * lux = data/100
	 */
	data = data / 100;
	data += drv_data->offset;
	if (data < 0)
		data = 1;

	v[0] = data * drv_data->scale + data * drv_data->uscale / 10000;
	v[1] = 0;
	v[2] = 0;

	/*
	 * Return an error when nothing change to prevent filling the
	 * fifo with useless data.
	 */
	if (v[0] == drv_data->last_value)
		return EC_ERROR_UNCHANGED;

	drv_data->last_value = v[0];
	return EC_SUCCESS;
}

static int cm32181e_set_range(const struct motion_sensor_t *s, int range,
			     int rnd)
{
	struct cm32181e_drv_data_t *drv_data = CM32181E_GET_DATA(s);

	drv_data->scale = range >> 16;
	drv_data->uscale = range & 0xffff;
	return EC_SUCCESS;
}

static int cm32181e_get_range(const struct motion_sensor_t *s)
{
	struct cm32181e_drv_data_t *drv_data = CM32181E_GET_DATA(s);

	return (drv_data->scale << 16) | (drv_data->uscale);
}

static int cm32181e_set_data_rate(const struct motion_sensor_t *s,
				int rate, int roundup)
{
	struct cm32181e_drv_data_t *drv_data = CM32181E_GET_DATA(s);

	drv_data->rate = rate;
	return EC_SUCCESS;
}

static int cm32181e_get_data_rate(const struct motion_sensor_t *s)
{
	struct cm32181e_drv_data_t *drv_data = CM32181E_GET_DATA(s);

	return drv_data->rate;
}

static int cm32181e_set_offset(const struct motion_sensor_t *s,
			const int16_t *offset,
			int16_t    temp)
{
	struct cm32181e_drv_data_t *drv_data = CM32181E_GET_DATA(s);

	drv_data->offset = offset[X];
	return EC_SUCCESS;
}

static int cm32181e_get_offset(const struct motion_sensor_t *s,
			int16_t   *offset,
			int16_t    *temp)
{
	struct cm32181e_drv_data_t *drv_data = CM32181E_GET_DATA(s);

	offset[X] = drv_data->offset;
	offset[Y] = 0;
	offset[Z] = 0;
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}
/**
 * Initialise CM32181E light sensor.
 */
static int cm32181e_init(const struct motion_sensor_t *s)
{
	/*
	 * [15-12]: 1100b Automatic full-scale setting mode
	 * [11]   : 1b    Conversion time 800ms
	 * [4]    : 1b    Latched window-style comparison operation
	 */
	cm32181e_i2c_write(s->port, s->addr, CM32181E_REG_CONFIGURE, 0x00C0);

	cm32181e_set_range(s, s->default_range, 0);

	return EC_SUCCESS;
}

const struct accelgyro_drv cm32181e_drv = {
	.init = cm32181e_init,
	.read = cm32181e_read_lux,
	.set_range = cm32181e_set_range,
	.get_range = cm32181e_get_range,
	.set_offset = cm32181e_set_offset,
	.get_offset = cm32181e_get_offset,
	.set_data_rate = cm32181e_set_data_rate,
	.get_data_rate = cm32181e_get_data_rate,
};
