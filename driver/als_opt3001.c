/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI OPT3001 light sensor driver
 */

#include "accelgyro.h"
#include "als.h"
#include "console.h"
#include "driver/als_opt3001.h"
#include "i2c.h"

#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

/**
 *  Read register from OPT3001 light sensor.
 */
static int opt3001_i2c_read(const int port, const int addr,
			    const int reg, int *data_ptr)
{
	int ret;

	ret = i2c_read16(port, addr, reg, data_ptr);
	if (!ret)
		*data_ptr = ((*data_ptr << 8) & 0xFF00) |
				((*data_ptr >> 8) & 0x00FF);

	return ret;
}

/**
 *  Write register to OPT3001 light sensor.
 */
static int opt3001_i2c_write(const int port, const int addr,
			     const int reg, int data)
{
	data = ((data << 8) & 0xFF00) | ((data >> 8) & 0x00FF);
	return i2c_write16(port, addr, reg, data);
}

/**
 * Initialise OPT3001 light sensor.
 */
static int opt3001_init(const struct motion_sensor_t *s)
{
	int data;
	int ret;

	ret = opt3001_i2c_read(s->port, s->addr, OPT3001_REG_MAN_ID, &data);
	if (ret)
		return ret;
	if (data != OPT3001_MANUFACTURER_ID)
		return EC_ERROR_UNKNOWN;

	ret = opt3001_i2c_read(s->port, s->addr, OPT3001_REG_DEV_ID, &data);
	if (ret)
		return ret;
	if (data != OPT3001_DEVICE_ID)
		return EC_ERROR_UNKNOWN;

	/*
	 * [15:12]: 1100b Automatic full scale
	 * [11]   : 1b    Conversion time 800ms
	 * [10:9] : 10b   Continuous Mode of conversion operation
	 * [4]    : 1b    Latched window-style comparison operation
	 */
	return opt3001_i2c_write(s->port, s->addr,
				 OPT3001_REG_CONFIGURE, 0xCC10);
}

/**
 * Read OPT3001 light sensor data.
 */
static int opt3001_read_lux(const struct motion_sensor_t *s, vector_3_t v)
{
	int ret;
	int data;
#ifdef HAS_TASK_ALS
	struct opt3001_drv_data_t *drv_data = OPT3001_GET_DATA(s);
#endif

	ret = opt3001_i2c_read(s->port, s->addr, OPT3001_REG_RESULT, &data);
	if (ret)
		return ret;

	/*
	 * 16-bit register has two fields: a 4-bit exponent [MSB] and a
	 * 12-bit mantissa [LSB].
	 * E[3:0]  exponent result.
	 * R[11:0] mantissa result.
	 *
	 * lux = 2EXP[3:0] × R[11:0] / 100
	 */

#ifdef HAS_TASK_ALS
	/*
	 * We multiply the sensora value by a scaling factor to account
	 * for attenuation by glass, tinting, etc.
	 */
	v[0] = (int) ((uint64_t)
		((1 << ((data & 0xF000) >> 12)) * (data & 0x0FFF) *
			drv_data->attenuation_factor) / 100);
#else
	v[0] = (int) ((uint64_t)
		((1 << ((data & 0xF000) >> 12)) * (data & 0x0FFF)) / 100);
#endif

	v[1] = 0;
	v[2] = 0;

	return EC_SUCCESS;
}

#ifdef HAS_TASK_ALS
const struct als_driver opt3001_drv = {
	.name = "TI_OPT3001",
	.port = I2C_PORT_ALS,
	.addr = OPT3001_I2C_ADDR,
	.init = &opt3001_init,
	.read = &opt3001_read_lux,
};
#else
static int opt3001_set_range(const struct motion_sensor_t *s, int range,
			     int rnd)
{
	int rv;
	int reg;
	struct opt3001_drv_data_t *drv_data = OPT3001_GET_DATA(s);

	if (range < 0 || range > OPT3001_RANGE_AUTOMATIC_FULL_SCALE)
		return EC_ERROR_INVAL;

	rv = opt3001_i2c_read(s->port, s->addr, OPT3001_REG_CONFIGURE, &reg);
	if (rv)
		return rv;

	rv = opt3001_i2c_write(s->port, s->addr, OPT3001_REG_CONFIGURE,
			       (reg & 0x0FFF) | (range << 12));
	if (rv)
		return rv;

	drv_data->range = range;

	return EC_SUCCESS;
}

static int opt3001_get_range(const struct motion_sensor_t *s)
{
	struct opt3001_drv_data_t *drv_data = OPT3001_GET_DATA(s);

	return drv_data->range;
}

static int opt3001_set_data_rate(const struct motion_sensor_t *s,
				int rate, int roundup)
{
	struct opt3001_drv_data_t *drv_data = OPT3001_GET_DATA(s);

	drv_data->rate = rate;
	return EC_SUCCESS;
}

static int opt3001_get_data_rate(const struct motion_sensor_t *s)
{
	struct opt3001_drv_data_t *drv_data = OPT3001_GET_DATA(s);

	return drv_data->rate;
}

const struct accelgyro_drv opt3001_drv = {
	.init = opt3001_init,
	.read = opt3001_read_lux,
	.set_range = opt3001_set_range,
	.get_range = opt3001_get_range,
	.set_data_rate = opt3001_set_data_rate,
	.get_data_rate = opt3001_get_data_rate,
};
#endif

struct opt3001_drv_data_t g_opt3001_data = {
#ifdef HAS_TASK_ALS
	.attenuation_factor = 1,
#else
	.range = OPT3001_RANGE_AUTOMATIC_FULL_SCALE,
#endif
};

#ifdef CONFIG_CMD_I2C_STRESS_TEST_ALS
struct i2c_stress_test_dev opt3001_i2c_stress_test_dev = {
	.reg_info = {
		.read_reg = OPT3001_REG_DEV_ID,
		.read_val = OPT3001_DEVICE_ID,
		.write_reg = OPT3001_REG_INT_LIMIT_LSB,
	},
	.i2c_read = &opt3001_i2c_read,
	.i2c_write = &opt3001_i2c_write,
};
#endif /* CONFIG_CMD_I2C_STRESS_TEST_ALS */
