/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * CAPELLA CM32181E light sensor driver
 */

#include "common.h"
#include "driver/als_cm32181e.h"
#include "i2c.h"
#include "accelgyro.h"
#include "math_util.h"

/*
 * Read data from CM32181E light sensor, and transfer unit into lux.
 */
static int cm32181e_read_lux(const struct motion_sensor_t *s, vector_3_t v)
{
	struct cm32181e_drv_data *drv_data = CM32181E_GET_DATA(s);
	int ret;
	int data;

	ret = i2c_read16(s->port, s->addr, CM32181E_REG_RESULT, &data);
	if (ret)
		return ret;

	/*
	 * lux = data/100
	 */
	data = data / 100;
	data += drv_data->offset;
	if (data < 0)
		data = 1;

	v[0] = data;
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
	return EC_SUCCESS;
}

static int cm32181e_get_range(const struct motion_sensor_t *s)
{
	/* 16 bit resolution */
	return s->default_range;
}

static int cm32181e_set_data_rate(const struct motion_sensor_t *s,
				int rate, int roundup)
{
	struct cm32181e_drv_data *drv_data = CM32181E_GET_DATA(s);

	drv_data->rate = rate;
	return EC_SUCCESS;
}

static int cm32181e_get_data_rate(const struct motion_sensor_t *s)
{
	return CM32181E_GET_DATA(s)->rate;
}

static int cm32181e_set_offset(const struct motion_sensor_t *s,
			const int16_t *offset,
			int16_t    temp)
{
	return EC_SUCCESS;
}

static int cm32181e_get_offset(const struct motion_sensor_t *s,
			int16_t   *offset,
			int16_t    *temp)
{
	*offset = 0;

	return EC_SUCCESS;
}
/**
 * Initialise CM32181E light sensor.
 */
static int cm32181e_init(const struct motion_sensor_t *s)
{
	/*
	 * [9-6]  : 0011b	Conversion time 800ms
	 */
	i2c_write16(s->port, s->addr, CM32181E_REG_CONFIGURE, 0x00C0);

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
