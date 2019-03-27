/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI TCS3400 light sensor driver
 */

#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/als_tcs3400.h"
#include "i2c.h"
#include "math_util.h"
#include "task.h"

#ifdef HAS_TASK_ALS
static inline int tcs3400_i2c_read(const int reg, int *data_ptr)
{
	return i2c_read8(I2C_PORT_TEST, TCS3400_I2C_ADDR, reg, data_ptr);
}

static inline int tcs3400_i2c_write(const int reg, int data)
{
	return i2c_write8(I2C_PORT_TEST, TCS3400_I2C_ADDR, reg, data);
}

/**
 * Read TCS3400 light sensor data.
 */
int tcs3400_read_lux(int *lux, int af)
{
	int ret;
	int data;
	int value = 0;

	ret = tcs3400_i2c_read(TCS_I2C_CDATAL, &data);
	if (ret)
		return ret;
	value = data;

	ret = tcs3400_i2c_read(TCS_I2C_CDATAH, &data);
	if (ret)
		return ret;
	value |= (data << 8);

	/*
	 * The default power-on values will give 12 bits of precision:
	 * 0x0000-0x0fff indicates 0 to 1310.40 lux. We multiply the sensor
	 * value by a scaling factor to account for attenuation by glass,
	 * tinting, etc.
	 */

	/*
	 * lux = 2EXP[3:0] × R[11:0] / 100
	 */
	*lux = (1 << ((value & 0xF000) >> 12)) * (value & 0x0FFF) * af / 100;

	return EC_SUCCESS;
}
#else /* HAS_TASK_ALS */

static inline int tcs3400_i2c_read_port(uint8_t port, uint8_t addr,
					const int reg, int *data_ptr)
{
	return i2c_read8(port, addr, reg, data_ptr);
}

static inline int tcs3400_i2c_write_port(uint8_t port, uint8_t addr,
					const int reg, int data)
{
	return i2c_write8(port, addr, reg, data);
}

int tcs3400_read(const struct motion_sensor_t *s, intv3_t v)
{
	int ret;
	int data;

	/* Enable the ADC to start cycle */
	ret = i2c_read8(s->port, s->addr, TCS_I2C_ENABLE, &data);
	if (ret == EC_SUCCESS)
		ret = i2c_write8(s->port, s->addr, TCS_I2C_ENABLE,
				(data | TCS_I2C_ENABLE_ADC_ENABLE |
				TCS_I2C_ENABLE_POWER_ON |
				TCS_I2C_ENABLE_INT_ENABLE));
	return ret;
}

int tcs3400_read_lux(const struct motion_sensor_t *s, intv3_t v)
{
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);
	int ret;
	int data = 0;
	int value = 0;

	/* Make sure data is valid */
	ret = tcs3400_i2c_read_port(s->port, s->addr, TCS_I2C_STATUS, &data);
	if (ret)
		return ret;
	if (!(data & TCS_I2C_STATUS_RGBC_VALID)) {
		cprints(CC_TASK, "%s RGBC not valid (0x%x)", __func__, data);
		return EC_ERROR_UNCHANGED;
	}

	ret = tcs3400_i2c_read_port(s->port, s->addr, TCS_I2C_CDATAL, &data);
	if (ret)
		return ret;
	value = data;

	ret = tcs3400_i2c_read_port(s->port, s->addr, TCS_I2C_CDATAH, &data);
	if (ret)
		return ret;
	value |= (data << 8);
	cprints(CC_TASK, "%s: read 0x%x from CDATA", __func__, value);

	/*
	 * lux = 2EXP[3:0] × R[11:0] / 100
	 */
	value = (1 << (value >> 12)) * (value & 0x0FFF);
	value += drv_data->offset * 100;
	value = value * drv_data->scale + value * drv_data->uscale / 10000;
	value /= 100;

	if (value < 0)
		value = 1;

	v[0] = value;
	v[1] = 0;
	v[2] = 0;

	/*
	 * Return an error when nothing change to prevent filling the
	 * fifo with useless data.
	 */
	if (v[0] == drv_data->last_value) {
		cprints(CC_TASK, "%s: value 0x%x unchanged", __func__, value);
		return EC_ERROR_UNCHANGED;
	}
	drv_data->last_value = v[0];

	cprints(CC_TASK, "%s: returning 0x%x", __func__, v[0]);
	return EC_SUCCESS;
}

/**
 * Read TCS3400 light sensor data.
 */
int tcs3400_read_rgb(const struct motion_sensor_t *s, intv3_t v)
{
	int ret;
	int data = 0;
	int x;
	int value;
	int retries = 20;	/* 400 ms max */
	uint8_t reg[3] = { TCS_I2C_RDATAL, TCS_I2C_GDATAL, TCS_I2C_BDATAL };

	/* Make sure data is valid */
	while (!(data & TCS_I2C_STATUS_RGBC_VALID)) {
		ret = tcs3400_i2c_read_port(s->port, s->addr,
						TCS_I2C_STATUS, &data);
		if (ret)
			return ret;
		if (!(data & TCS_I2C_STATUS_RGBC_VALID)) {
			retries--;
			if (retries == 0)
				return EC_ERROR_UNCHANGED;
			cprints(CC_TASK, "%s RGBC not valid (0x%x)",
				__func__, data);
			msleep(20);
		}
	}

	/* Read RGB registers */
	for (x = 0; x < 3; x++) {
		/* read the low data register */
		ret = tcs3400_i2c_read_port(s->port, s->addr, reg[x], &data);
		if (ret)
			return ret;
		value = data;

		/* read the high data register */
		ret = tcs3400_i2c_read_port(s->port, s->addr, reg[x]+1, &data);
		if (ret)
			return ret;
		value |= (data << 8);

		cprints(CC_TASK, "%s: v[%d] = 0x%x", __func__, x, value);
		v[x] = value;
	}

	return EC_SUCCESS;
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
	intv3_t v;
	int status = 0;
	int ret = EC_SUCCESS;

	if (!(*event & CONFIG_ALS_TCS3400_INT_EVENT))
		return EC_ERROR_NOT_HANDLED;

	ret = i2c_read8(s->port, s->addr, TCS_I2C_STATUS, &status);
	if (ret)
		return ret;

	if ((status & TCS_I2C_STATUS_RGBC_VALID) ||
		((status & TCS_I2C_STATUS_ALS_IRQ) &&
		(status & TCS_I2C_STATUS_ALS_VALID))) {
		if (s->type == MOTIONSENSE_TYPE_LIGHT)
			tcs3400_read_lux(s, v);
		else if (s->type == MOTIONSENSE_TYPE_LIGHT_EXT)
			tcs3400_read_rgb(s, v);

		if ((status & TCS_I2C_STATUS_ALS_IRQ) &&
			(status & TCS_I2C_STATUS_ALS_VALID)) {
			cprints(CC_TASK, "%s clearing CICLEAR", __func__);
			ret = i2c_write8(s->port, s->addr, TCS_I2C_CICLEAR, 0);
			if (ret)
				return ret;
		}
	}

	ret = i2c_write8(s->port, s->addr, TCS_I2C_AICLEAR, 0);
	if (ret)
		return ret;

	ret = i2c_read8(s->port, s->addr, TCS_I2C_ENABLE, &status);
	if (ret)
		return ret;
	ret = i2c_write8(s->port, s->addr, TCS_I2C_ENABLE,
			(status & ~TCS_I2C_ENABLE_INT_ENABLE));

	return ret;
}

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

static int tcs3400_get_data_rate(const struct motion_sensor_t *s)
{
	/* Sensor in forced mode, rate is used by motion_sense */
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);

	return drv_data->rate;
}

static int tcs3400_set_data_rate(const struct motion_sensor_t *s,
				int rate,
				int rnd)
{
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);

	drv_data->rate = rate;
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

	cprints(CC_TASK, "%s", __func__);
	ret = tcs3400_i2c_read_port(s->port, s->addr, TCS_I2C_ID, &data);
	if (ret)
		return ret;
	if (data != TCS3400_DEVICE_ID)
		return EC_ERROR_ACCESS_DENIED;

	tcs3400_set_range(s, s->default_range, 0);
	return ret;
}

const struct accelgyro_drv tcs3400_drv = {
	.init = tcs3400_init,
	.read = tcs3400_read,
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
	.init = tcs3400_init,
	.read = tcs3400_read,
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
		.read_reg = TCS_I2C_ID,
		.read_val = TCS3400_DEVICE_ID,
		.write_reg = TCS3400_REG_INT_LIMIT_LSB,
	},
	.i2c_read = &tcs3400_i2c_read,
	.i2c_write = &tcs3400_i2c_write,
};
#endif  /* CONFIG_CMD_I2C_STRESS_TEST_ALS */
#endif  /* HAS_TASK_ALS */
