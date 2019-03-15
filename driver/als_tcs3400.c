/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI TCS3400 light sensor driver
 */

#include "common.h"
#include "driver/als_tcs3400.h"
#include "i2c.h"
#include "accelgyro.h"
#include "math_util.h"


/**
 * Initialise TCS3400 light sensor.
 */
int tcs3400_init(void)
{
	int data;
	int ret;

	ret = tcs3400_i2c_read(TCS3400_REG_MAN_ID, &data);
	if (ret)
		return ret;
	if (data != TCS3400_MANUFACTURER_ID)
		return EC_ERROR_UNKNOWN;

	ret = tcs3400_i2c_read(TCS3400_REG_DEV_ID, &data);
	if (ret)
		return ret;
	if (data != TCS3400_DEVICE_ID)
		return EC_ERROR_UNKNOWN;

	/*
	 * [15:12]: 0101b Automatic full scale (1310.40lux, 0.32lux/lsb)
	 * [11]   : 1b    Conversion time 800ms
	 * [10:9] : 10b   Continuous Mode of conversion operation
	 * [4]    : 1b    Latched window-style comparison operation
	 */
	return tcs3400_i2c_write(TCS3400_REG_CONFIGURE, 0x5C10);
}

/**
 * Read TCS3400 light sensor data.
 */
int tcs3400_read_lux(int *lux, int af)
{
	int ret;
	int data;

	ret = tcs3400_i2c_read(TCS3400_REG_RESULT, &data);
	if (ret)
		return ret;

	/*
	 * The default power-on values will give 12 bits of precision:
	 * 0x0000-0x0fff indicates 0 to 1310.40 lux. We multiply the sensor
	 * value by a scaling factor to account for attenuation by glass,
	 * tinting, etc.
	 */

	/*
	 * lux = 2EXP[3:0] × R[11:0] / 100
	 */
	*lux = BIT((data & 0xF000) >> 12)) * (data & 0x0FFF) * af / 100;

	return EC_SUCCESS;
}

/**
 * Read TCS3400 light sensor data.
 */
int tcs3400_read_lux(const struct motion_sensor_t *s, intv3_t v)
{
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);
	int ret;
	int data;

	ret = tcs3400_i2c_read(s->port, s->addr, TCS3400_REG_RESULT, &data);
	if (ret)
		return ret;

	/*
	 * lux = 2EXP[3:0] × R[11:0] / 100
	 */
	data = BIT(data >> 12)) * (data & 0x0FFF);
	data += drv_data->offset * 100;
	data = data * drv_data->scale + data * drv_data->uscale / 10000;
	data /= 100;

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
	else {
		drv_data->last_value = v[0];
		return EC_SUCCESS;
	}
}
void tcs3400_interrupt(enum gpio_signal signal)
{
	task_set_event(TASK_ID_MOTIONSENSE,
		       CONFIG_ALS_TCS3400_INT_EVENT, 0);
}

/**
 * irq_handler - bottom half of the interrupt stack.
 * Ran from the motion_sense task, finds the events that raised the interrupt.
 *
 * For now, we just print out. We should set a bitmask motion sense code will
 * act upon.
 */
static int irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	uint8_t status;
	

static int tcs3400_set_range(const struct motion_sensor_t *s, int range,
			     int rnd)
{
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);

	drv_data->scale = range >> 16;
	drv_data->uscale = range & 0xffff;
	return EC_SUCCESS;
}

static int tcs3400_get_range(const struct motion_sensor_t *s)
{
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);

	return (drv_data->scale << 16) | (drv_data->uscale);
}

static int get_data_rate(const struct motion_sensor_t *s)
{
	/* Sensor in forced mode, rate is used by motion_sense */
	struct si114x_typed_data_t *data = SI114X_GET_TYPED_DATA(s);
	return data->rate;
}

static int set_data_rate(const struct motion_sensor_t *s,
				int rate,
				int rnd)
{
	struct si114x_typed_data_t *data = SI114X_GET_TYPED_DATA(s);
	data->rate = rate;
	return EC_SUCCESS;
}

static int tcs3400_set_offset(const struct motion_sensor_t *s,
			const int16_t *offset,
			int16_t    temp)
{
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);

	drv_data->offset = offset[X];
	return EC_SUCCESS;
}

static int tcs3400_get_offset(const struct motion_sensor_t *s,
			int16_t   *offset,
			int16_t    *temp)
{
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);

	offset[X] = drv_data->offset;
	offset[Y] = 0;
	offset[Z] = 0;
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}
/**
 * Initialise TCS3400 light sensor.
 */
static int tcs3400_init(const struct motion_sensor_t *s)
{
	int data;
	int ret;

	if (s->type == MOTIONSENSE_TYPE_LIGHT) {



	ret = tcs3400_i2c_read(s->port, s->addr, TCS3400_REG_MAN_ID, &data);
	if (ret)
		return ret;
	if (data != TCS3400_MANUFACTURER_ID)
		return EC_ERROR_ACCESS_DENIED;

	ret = tcs3400_i2c_read(s->port, s->addr, TCS3400_REG_DEV_ID, &data);
	if (ret)
		return ret;
	if (data != TCS3400_DEVICE_ID)
		return EC_ERROR_ACCESS_DENIED;

	/*
	 * [15-12]: 1100b Automatic full-scale setting mode
	 * [11]   : 1b    Conversion time 800ms
	 * [4]    : 1b    Latched window-style comparison operation
	 */
	tcs3400_i2c_write(s->port, s->addr, TCS3400_REG_CONFIGURE, 0xC810);

	tcs3400_set_range(s, s->default_range, 0);

	return EC_SUCCESS;
}

const struct accelgyro_drv tcs3400_drv = {
	.init = tcs3400_init,
	.read = tcs3400_read_clear,
	.set_range = tcs3400_set_range,
	.get_range = tcs3400_get_range,
	.set_offset = tcs3400_set_offset,
	.get_offset = tcs3400_get_offset,
	.set_data_rate = tcs3400_set_data_rate,
	.get_data_rate = tcs3400_get_data_rate,
#ifdef CONFIG_ACCEL_INTERRUPTS
	.irq_handler = irq_handler,
#endif
};

const struct accelgyro_drv tcs3400_rgb_drv = {
	.init = tcs3400_init_noop,
	.read = tcs3400_read_rgb,
	.set_range = tcs3400_set_range,
	.get_range = tcs3400_get_range,
	.set_offset = tcs3400_set_offset,
	.get_offset = tcs3400_get_offset,
	.set_data_rate = tcs3400_set_data_rate,
	.get_data_rate = tcs3400_get_data_rate,
};

#ifdef CONFIG_CMD_I2C_STRESS_TEST_ALS
struct i2c_stress_test_dev tcs3400_i2c_stress_test_dev = {
	.reg_info = {
		.read_reg = TCS3400_REG_DEV_ID,
		.read_val = TCS3400_DEVICE_ID,
		.write_reg = TCS3400_REG_INT_LIMIT_LSB,
	},
	.i2c_read = &tcs3400_i2c_read,
	.i2c_write = &tcs3400_i2c_write,
};
#endif  /* CONFIG_CMD_I2C_STRESS_TEST_ALS */
#endif  /* HAS_TASK_ALS */
