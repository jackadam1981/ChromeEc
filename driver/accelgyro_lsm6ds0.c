/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LSM6DS0 accelerometer and gyro module for Chrome EC */

#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/accelgyro_lsm6ds0.h"
#include "hooks.h"
#include "i2c.h"
#include "shared_mem.h"
#include "task.h"
#include "util.h"

/*
 * Struct for pairing an engineering value with the register value for a
 * parameter.
 */
struct accel_param_pair {
	int val; /* Value in engineering units. */
	int reg_val; /* Corresponding register value. */
};

#define RANGE_TBL_SIZE 3

/* List of range values in +/-G's and their associated register values. */
static const struct accel_param_pair g_ranges[RANGE_TBL_SIZE] = {
	{2, LSM6DS0_GSEL_2G},
	{4, LSM6DS0_GSEL_4G},
	{8, LSM6DS0_GSEL_8G}
};

/*
 * List of angular rate range values in +/-dps's
 * and their associated register values.
 */
const struct accel_param_pair dps_ranges[RANGE_TBL_SIZE] = {
	{245, LSM6DS0_DPS_SEL_245},
	{500, LSM6DS0_DPS_SEL_500},
	{2000, LSM6DS0_DPS_SEL_2000}
};

static inline const struct accel_param_pair *get_range_table(
		enum sensor_type_t type)
{
	return (SENSOR_ACCELEROMETER == type) ?
			g_ranges : dps_ranges;
}

#define ODR_TBL_SIZE 6

/* List of ODR (gyro off) values in mHz and their associated register values.*/
const struct accel_param_pair gyro_on_odr[ODR_TBL_SIZE] = {
	{15000,    LSM6DS0_ODR_15HZ},
	{59000,    LSM6DS0_ODR_59HZ},
	{119000,   LSM6DS0_ODR_119HZ},
	{238000,   LSM6DS0_ODR_238HZ},
	{476000,   LSM6DS0_ODR_476HZ},
	{952000,   LSM6DS0_ODR_952HZ}
};

/* List of ODR (gyro on) values in mHz and their associated register values. */
const struct accel_param_pair gyro_off_odr[ODR_TBL_SIZE] = {
	{10000,    LSM6DS0_ODR_10HZ},
	{50000,    LSM6DS0_ODR_50HZ},
	{119000,   LSM6DS0_ODR_119HZ},
	{238000,   LSM6DS0_ODR_238HZ},
	{476000,   LSM6DS0_ODR_476HZ},
	{952000,   LSM6DS0_ODR_952HZ}
};

static inline const struct accel_param_pair *get_odr_table(
		enum sensor_type_t type)
{
	return (SENSOR_ACCELEROMETER == type) ?
			gyro_on_odr : gyro_off_odr;
}

static inline int get_ctrl_reg(enum sensor_type_t type)
{
	return (SENSOR_ACCELEROMETER == type) ?
		LSM6DS0_CTRL_REG6_XL : LSM6DS0_CTRL_REG1_G;
}

static inline int get_xyz_reg(enum sensor_type_t type)
{
	return (SENSOR_ACCELEROMETER == type) ?
		LSM6DS0_OUT_X_L_XL : LSM6DS0_OUT_X_L_G;
}

/**
 * @return reg value that matches the given engineering value passed in.
 * The round_up flag is used to specify whether to round up or down.
 * Note, this function always returns a valid reg value. If the request is
 * outside the range of values, it returns the closest valid reg value.
 */
static int get_reg_val(const int eng_val, const int round_up,
		const struct accel_param_pair *pairs, const int size)
{
	int i;
	for (i = 0; i < size - 1; i++) {
		if (eng_val <= pairs[i].val)
			break;

		if (eng_val < pairs[i+1].val) {
			if (round_up)
				i += 1;
			break;
		}
	}
	return pairs[i].reg_val;
}

/**
 * @return engineering value that matches the given reg val
 */
static int get_engineering_val(const int reg_val,
		const struct accel_param_pair *pairs, const int size)
{
	int i;
	for (i = 0; i < size; i++) {
		if (reg_val == pairs[i].reg_val)
			break;
	}
	return pairs[i].val;
}

/**
 * Read register from accelerometer.
 */
static inline int raw_read8(const int addr, const int reg, int *data_ptr)
{
	return i2c_read8(I2C_PORT_ACCEL, addr, reg, data_ptr);
}

/**
 * Write register from accelerometer.
 */
static inline int raw_write8(const int addr, const int reg, int data)
{
	return i2c_write8(I2C_PORT_ACCEL, addr, reg, data);
}

static int set_range(struct motion_sensor_t *s,
				const int range,
				const int rnd)
{
	int ret, ctrl_val, ctrl_reg, reg_val;
	const struct accel_param_pair *ranges;

	ctrl_reg = get_ctrl_reg(s->type);
	ranges = get_range_table(s->type);

	reg_val = get_reg_val(range, rnd, ranges, RANGE_TBL_SIZE);

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(s->mutex);

	ret = raw_read8(s->i2c_addr, ctrl_reg, &ctrl_val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	ctrl_val = (ctrl_val & ~LSM6DS0_RANGE_MASK) | reg_val;
	ret = raw_write8(s->i2c_addr, ctrl_reg, ctrl_val);

accel_cleanup:
	/* Unlock accel resource and save new range if written successfully. */
	mutex_unlock(s->mutex);
	return EC_SUCCESS;
}

static int get_range(struct motion_sensor_t *s, int * const range)
{
	int ret, ctrl_reg, ctrl_val;
	const struct accel_param_pair *ranges;
	ranges = get_range_table(s->type);
	ctrl_reg = get_ctrl_reg(s->type);
	ret = raw_read8(s->i2c_addr, ctrl_reg, &ctrl_val);
	*range = get_engineering_val(ctrl_val & LSM6DS0_RANGE_MASK,
		ranges, RANGE_TBL_SIZE);
	return ret;
}

static int set_resolution(struct motion_sensor_t *s,
				const int res,
				const int rnd)
{
	/* Only one resolution, LSM6DS0_RESOLUTION, so nothing to do. */
	return EC_SUCCESS;
}

static int get_resolution(struct motion_sensor_t *s,
				int * const res)
{
	*res = LSM6DS0_RESOLUTION;
	return EC_SUCCESS;
}

static int set_data_rate(struct motion_sensor_t *s,
				const int rate,
				const int rnd)
{
	int ret, ctrl_val, ctrl_reg, reg_val;
	const struct accel_param_pair *data_rates;

	ctrl_reg = get_ctrl_reg(s->type);
	data_rates = get_range_table(s->type);
	reg_val = get_reg_val(rate, rnd, data_rates, ODR_TBL_SIZE);

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(s->mutex);

	ret = raw_read8(s->i2c_addr, ctrl_reg, &ctrl_val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	ctrl_val = (ctrl_val & ~LSM6DS0_ODR_MASK) | reg_val;
	ret = raw_write8(s->i2c_addr, ctrl_reg, ctrl_val);

accel_cleanup:
	/* Unlock accel resource and save new ODR if written successfully. */
	mutex_unlock(s->mutex);
	return EC_SUCCESS;
}

static int get_data_rate(struct motion_sensor_t *s,
			      int * const rate)
{
	int ret, ctrl_reg, ctrl_val;
	const struct accel_param_pair *data_rates;
	ctrl_reg = get_ctrl_reg(s->type);

	ret = raw_read8(s->i2c_addr, ctrl_reg, &ctrl_val);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	data_rates = get_range_table(s->type);
	*rate = get_engineering_val(ctrl_val & LSM6DS0_ODR_MASK,
			data_rates, ODR_TBL_SIZE);
	return EC_SUCCESS;
}

#ifdef CONFIG_ACCEL_INTERRUPTS
static int set_interrupt(struct motion_sensor_t *s,
			       unsigned int threshold)
{
	/* Currently unsupported. */
	return EC_ERROR_UNKNOWN;
}
#endif

static int read(struct motion_sensor_t *s,
		      int * const x_acc,
		      int * const y_acc,
		      int * const z_acc)
{
	uint8_t acc[6];
	uint8_t xyz_reg;
	int ret, multiplier;
	int range;

	xyz_reg = get_xyz_reg(s->type);

	/* Read 6 bytes starting at xyz_reg */
	i2c_lock(I2C_PORT_ACCEL, 1);
	ret = i2c_xfer(I2C_PORT_ACCEL, s->i2c_addr,
			&xyz_reg, 1, acc, 6, I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_ACCEL, 0);

	if (ret != EC_SUCCESS)
		return ret;

	ret = get_range(s, &range);
	if (ret != EC_SUCCESS)
		return ret;

	*x_acc = ((int16_t)(acc[1] << 8 | acc[0]));
	*y_acc = ((int16_t)(acc[3] << 8 | acc[2]));
	*z_acc = ((int16_t)(acc[5] << 8 | acc[4]));

	if (SENSOR_ACCELEROMETER == s->type) {
		/* Convert data to signed 12-bit value */
		*x_acc >>= 4;
		*y_acc >>= 4;
		*z_acc >>= 4;

		/* scale up */
		multiplier = range >> 1;
		*x_acc *= multiplier;
		*y_acc *= multiplier;
		*z_acc *= multiplier;
	}

	return EC_SUCCESS;
}

static int init(struct motion_sensor_t *s)
{
	int ret;
	/*
	 * This sensor can be powered through an EC reboot, so the state of
	 * the sensor is unknown here. Initiate software reset to restore
	 * sensor to default.
	 */
	ret = raw_write8(s->i2c_addr, LSM6DS0_CTRL_REG8, 1);
	if (ret)
		return EC_ERROR_UNKNOWN;

	if (SENSOR_ACCELEROMETER == s->type) {
		ret = set_range(s, 2, 1);
		if (ret)
			return EC_ERROR_UNKNOWN;
		ret = set_data_rate(s, 100000, 1);
		if (ret)
			return EC_ERROR_UNKNOWN;
	} else if (SENSOR_GYRO == s->type) {
		ret = set_range(s, 245, 1);
		if (ret)
			return EC_ERROR_UNKNOWN;
		ret = set_data_rate(s, 100000, 1);
		if (ret)
			return EC_ERROR_UNKNOWN;
	}

	return ret;
}

struct accelgyro_method lsm6ds0_method = {
	.init = init,
	.read = read,
	.set_range = set_range,
	.get_range = get_range,
	.set_resolution = set_resolution,
	.get_resolution = get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = get_data_rate,
#ifdef CONFIG_ACCEL_INTERRUPTS
	.set_interrupt = set_interrupt,
#endif
};
