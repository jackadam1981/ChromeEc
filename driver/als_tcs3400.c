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
#include "hwtimer.h"
#include "i2c.h"
#include "math_util.h"
#include "task.h"

#ifdef CONFIG_ACCEL_FIFO
static volatile uint32_t last_interrupt_timestamp;
#endif

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

static int tcs3400_read(const struct motion_sensor_t *s, intv3_t v)
{
	int ret;
	int data;

	/* Enable the ADC to start cycle */
	ret = i2c_read8(s->port, s->addr, TCS_I2C_ENABLE, &data);
	if (ret)
		return ret;

	/* AND with ~0xA4 to assure writing 0 to reserved bits */
	data &= ~0xA4;
	ret = i2c_write8(s->port, s->addr, TCS_I2C_ENABLE,
			(data | TCS_I2C_ENABLE_ADC_ENABLE |
			TCS_I2C_ENABLE_POWER_ON |
			TCS_I2C_ENABLE_INT_ENABLE));
	if (ret == EC_SUCCESS)
		return EC_RES_IN_PROGRESS;
	return ret;
}

static int tcs3400_rgb_read(const struct motion_sensor_t *s, intv3_t v)
{
	return EC_SUCCESS;
}

static int tcs3400_post_events(struct motion_sensor_t *s, uint32_t last_ts)
{
	struct tcs3400_rgb_drv_data_t *rgb_drv_data = TCS3400_RGB_GET_DATA(s);
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);
	struct ec_response_motion_sensor_data vector;
	int *v = s->raw_xyz;
	int data = 0;
	int rgb_data[3];
	int ret;
	int retries = 20;	/* 400 ms max */
	uint8_t light_data[TCS_RGBC_DATA_SIZE];

	/*
	 * Rule says RGB sensor is right after ALS sensor, and this
	 * routine will only get called from ALS sensor driver.
	 */
	struct motion_sensor_t *rgb_s = s + 1;

	/* Make sure data is valid */
	do {
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
	} while (!(data & TCS_I2C_STATUS_RGBC_VALID));

	/* Read the light registers */
	ret = i2c_read_block(s->port, s->addr, TCS_DATA_START_LOCATION,
			light_data, TCS_RGBC_DATA_SIZE);
	if (ret)
		return ret;

	/* Transfer Clear data into sensor struct and into fifo */
	/* TODO - convert raw to lux ?? */
	data = ((light_data[1] << 8) | light_data[0]);
	data += drv_data->offset;
	if (data != drv_data->last_value) {
		drv_data->last_value = data;
		vector.flags = 0;
#ifdef CONFIG_ACCEL_SPOOF_MODE
		if (s->in_spoof_mode)
			v = s->spoof_xyz;
#endif  /* defined(CONFIG_ACCEL_SPOOF_MODE) */
		vector.data[X] = v[X] = data;
		vector.data[Y] = v[Y] = 0;
		vector.data[Z] = v[Z] = 0;
		vector.sensor_num = s - motion_sensors;
		cprints(CC_TASK, "%s Sending Clear channel data (0x%x)",
				__func__, data);
		motion_sense_fifo_add_data(&vector, s, 3, last_ts);
	} else {
		cprints(CC_TASK, "%s Clear channel data unchanged (0x%x)",
				__func__, data);
	}

	/* Transfer RGB data into sensor struct and into fifo */
	rgb_data[X] = ((light_data[3] << 8) | light_data[2]);
	rgb_data[X] += rgb_drv_data->offset[X];
	rgb_data[Y] = ((light_data[5] << 8) | light_data[4]);
	rgb_data[Y] += rgb_drv_data->offset[Y];
	rgb_data[Z] = ((light_data[7] << 8) | light_data[6]);
	rgb_data[Z] += rgb_drv_data->offset[Z];
	if ((rgb_drv_data->last_value[X] != rgb_data[X]) ||
		(rgb_drv_data->last_value[Y] != rgb_data[Y]) ||
		(rgb_drv_data->last_value[Z] != rgb_data[Z])) {
		for (int x = 0; x < 3; x++)
			rgb_drv_data->last_value[x] = rgb_data[x];
		v = rgb_s->raw_xyz;
		vector.flags = 0;
#ifdef CONFIG_ACCEL_SPOOF_MODE
		if (rgb_s->in_spoof_mode)
			v = rgb_s->spoof_xyz;
#endif  /* defined(CONFIG_ACCEL_SPOOF_MODE) */
		vector.data[X] = v[X] = rgb_data[X];
		vector.data[Y] = v[Y] = rgb_data[Y];
		vector.data[Z] = v[Z] = rgb_data[Z];
		vector.sensor_num = rgb_s - motion_sensors;
		cprints(CC_TASK, "%s Sending RGB channel data (0x%x 0x%x 0x%x)",
				__func__, v[X], v[Y], v[Z]);
		motion_sense_fifo_add_data(&vector, rgb_s, 3, last_ts);
	} else {
		cprints(CC_TASK, "%s RGB channel unchanged (0x%x 0x%x 0x%x)",
			__func__, rgb_data[X], rgb_data[Y], rgb_data[Z]);
	}

	return EC_SUCCESS;
}

void tcs3400_interrupt(enum gpio_signal signal)
{
#ifdef CONFIG_ACCEL_FIFO
	last_interrupt_timestamp = __hw_clock_source_read();
#endif

	task_set_event(TASK_ID_MOTIONSENSE,
		       CONFIG_ALS_TCS3400_INT_EVENT, 0);
}

/**
 * tcs3400_irq_handler - bottom half of the interrupt stack.
 * Ran from the motion_sense task, finds the events that raised the interrupt.
 *
 * For now, we just print out. We should set a bitmask motion sense code will
 * act upon.
 */
static int tcs3400_irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	int status = 0;
	int ret = EC_SUCCESS;

	if (!(*event & CONFIG_ALS_TCS3400_INT_EVENT))
		return EC_ERROR_NOT_HANDLED;

	ret = i2c_read8(s->port, s->addr, TCS_I2C_STATUS, &status);
	if (ret)
		return ret;
	cprints(CC_TASK, "%s: status=0x%x", __func__, status);

	if ((status & TCS_I2C_STATUS_RGBC_VALID) ||
		((status & TCS_I2C_STATUS_ALS_IRQ) &&
		(status & TCS_I2C_STATUS_ALS_VALID))) {
		/* Disable future interrupts */
		ret = i2c_read8(s->port, s->addr, TCS_I2C_ENABLE, &status);
		if (ret)
			return ret;
		ret = i2c_write8(s->port, s->addr, TCS_I2C_ENABLE,
				(status & ~TCS_I2C_ENABLE_INT_ENABLE));

#ifdef CONFIG_ACCEL_FIFO
		tcs3400_post_events(s, last_interrupt_timestamp);
#endif

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

	return ret;
}

static int tcs3400_rgb_get_range(const struct motion_sensor_t *s)
{
	struct tcs3400_rgb_drv_data_t *drv_data = TCS3400_RGB_GET_DATA(s);

	return (drv_data->scale << 16) | (drv_data->uscale);
}

static int tcs3400_rgb_set_range(const struct motion_sensor_t *s, int range,
			     int rnd)
{
	/* noop for now - TODO */
	return EC_SUCCESS;
}

static int tcs3400_rgb_get_offset(const struct motion_sensor_t *s,
			int16_t *offset,
			int16_t *temp)
{
	struct tcs3400_rgb_drv_data_t *drv_data = TCS3400_RGB_GET_DATA(s);

	offset[X] = drv_data->offset[Y];
	offset[Y] = drv_data->offset[Y];
	offset[Z] = drv_data->offset[Z];
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int tcs3400_rgb_set_offset(const struct motion_sensor_t *s,
			const int16_t *offset,
			int16_t temp)
{
	struct tcs3400_rgb_drv_data_t *drv_data = TCS3400_RGB_GET_DATA(s);

	drv_data->offset[X] = offset[X];
	drv_data->offset[Y] = offset[Y];
	drv_data->offset[Z] = offset[Z];
	return EC_SUCCESS;
}


static int tcs3400_get_range(const struct motion_sensor_t *s)
{
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);

	return (drv_data->scale << 16) | (drv_data->uscale);
}

static int tcs3400_set_range(const struct motion_sensor_t *s, int range,
			     int rnd)
{
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);

	drv_data->scale = range >> 16;
	drv_data->uscale = range & 0xffff;
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

static int tcs3400_set_offset(const struct motion_sensor_t *s,
			const int16_t *offset,
			int16_t    temp)
{
	struct tcs3400_drv_data_t *drv_data = TCS3400_GET_DATA(s);

	drv_data->offset = offset[X];
	return EC_SUCCESS;
}

/**
 * Initialise TCS3400 light sensor.
 */
static int tcs3400_rgb_init(const struct motion_sensor_t *s)
{
	return EC_SUCCESS;
}

static int tcs3400_init(const struct motion_sensor_t *s)
{
	int data = 0;
	int ret;
	int index_count;
	uint8_t defaults[][2] = {
		{ TCS_I2C_ENABLE, 0 },
		{ TCS_I2C_ATIME, 0xFF },
		{ TCS_I2C_WTIME, 0xFF },
		{ TCS_I2C_AILTL, 0 },
		{ TCS_I2C_AILTH, 0 },
		{ TCS_I2C_AIHTL, 0 },
		{ TCS_I2C_AIHTH, 0 },
		{ TCS_I2C_PERS, 0 },
		{ TCS_I2C_CONFIG, 0x40 },
		{ TCS_I2C_CONTROL, 0 },
		{ TCS_I2C_AUX, 0 },
		{ TCS_I2C_IR, 0 },
		{ TCS_I2C_CICLEAR, 0 },
		{ TCS_I2C_AICLEAR, 0 } };

	cprints(CC_TASK, "%s", __func__);
	ret = tcs3400_i2c_read_port(s->port, s->addr, TCS_I2C_ID, &data);
	if (ret) {
		cprints(CC_TASK, "%s failed reading ID reg 0x%x, ret=%d",
				__func__, TCS_I2C_ID, ret);
		return ret;
	}
	if (data != TCS3400_DEVICE_ID) {
		cprints(CC_TASK, "%s no ID match - data=0x%x", __func__, data);
		return EC_ERROR_ACCESS_DENIED;
	}

	/* reset chip to default power-on settings */
	index_count = sizeof(defaults) / sizeof(uint8_t);
	for (int x = 0; x < index_count; x++) {
		ret = tcs3400_i2c_write_port(s->port, s->addr,
				defaults[x][0], defaults[x][1]);
		if (ret)
			return ret;
	}

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
#ifdef CONFIG_ACCEL_INTERRUPTS
	.irq_handler = tcs3400_irq_handler,
#endif
};

const struct accelgyro_drv tcs3400_rgb_drv = {
	.init = tcs3400_rgb_init,
	.read = tcs3400_rgb_read,
	.set_range = tcs3400_rgb_set_range,
	.get_range = tcs3400_rgb_get_range,
	.set_offset = tcs3400_rgb_set_offset,
	.get_offset = tcs3400_rgb_get_offset,
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
