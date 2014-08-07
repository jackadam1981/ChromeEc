/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LSM6DS0 accelerometer and gyro module for Chrome EC */

#include "accelerometer.h"
#include "gyroscope.h"
#include "common.h"
#include "console.h"
#include "driver/accelgyro_lsm6ds0.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "util.h"

/*
 * Struct for pairing an engineering value with the register value for a
 * parameter.
 */
struct accel_param_pair {
	int val; /* Value in engineering units. */
	int reg; /* Corresponding register value. */
};

/* List of range values in +/-G's and their associated register values. */
const struct accel_param_pair g_ranges[] = {
	{2, LSM6DS0_GSEL_2G},
	{4, LSM6DS0_GSEL_4G},
	{8, LSM6DS0_GSEL_8G}
};

/*
 * List of angular rate range values in +/-dps's
 * and their associated register values.
 */
const struct accel_param_pair dps_ranges[] = {
	{245, LSM6DS0_DPS_SEL_245},
	{500, LSM6DS0_DPS_SEL_500},
	{2000, LSM6DS0_DPS_SEL_2000}
};

#define LA_ODR_TBL_SIZE 6

/* List of ODR (gyro off) values in mHz and their associated register values.*/
const struct accel_param_pair gyro_on_odr[LA_ODR_TBL_SIZE] = {
	{15000,    LSM6DS0_ODR_15HZ},
	{59000,    LSM6DS0_ODR_59HZ},
	{119000,   LSM6DS0_ODR_119HZ},
	{238000,   LSM6DS0_ODR_238HZ},
	{476000,   LSM6DS0_ODR_476HZ},
	{952000,   LSM6DS0_ODR_952HZ}
};

/* List of ODR (gyro on) values in mHz and their associated register values. */
const struct accel_param_pair gyro_off_odr[LA_ODR_TBL_SIZE] = {
	{10000,    LSM6DS0_ODR_10HZ},
	{50000,    LSM6DS0_ODR_50HZ},
	{119000,   LSM6DS0_ODR_119HZ},
	{238000,   LSM6DS0_ODR_238HZ},
	{476000,   LSM6DS0_ODR_476HZ},
	{952000,   LSM6DS0_ODR_952HZ}
};

/*
 * Current linear acceleration range of each accelerometer.
 * The value is an index into g_ranges[].
 */
static int sensor_g_range[ACCEL_COUNT] = {0, 0};

/*
 * Current angular rate range of each gyro.
 * The value is an index into dps_ranges[].
 */
static int sensor_dps_range[ACCEL_COUNT] = {0, 0};

/*
 * Current output data rate of each accelerometer.
 * The value is an index into la_odr_tbl[].
 */
static int la_sensor_datarate[ACCEL_COUNT] = {1, 1};

/*
 * Current output data rate of each gyro.
 * The value is an index into la_odr_tbl[].
 */
static int gyro_sensor_datarate[ACCEL_COUNT] = {1, 1};

static struct mutex lsm6ds0_mutex[ACCEL_COUNT];

static const struct accel_param_pair *g_la_odr_tbl[ACCEL_COUNT];

/**
 * Get LA ODR Table by device id
 */
static inline const struct accel_param_pair *get_la_odr_tbl(
	const enum accel_id id)
{
	return g_la_odr_tbl[id];
}

/*
 * Set LA ODR Table by device id
 */
static inline void set_la_odr_tbl(const enum accel_id id,
		const struct accel_param_pair *tbl)
{
	g_la_odr_tbl[id] = tbl;
}

/**
 * Find index into a accel_param_pair that matches the given engineering value
 * passed in. The round_up flag is used to specify whether to round up or down.
 * Note, this function always returns a valid index. If the request is
 * outside the range of values, it returns the closest valid index.
 */
static int find_param_index(const int eng_val, const int round_up,
		const struct accel_param_pair *pairs, const int size)
{
	int i;

	/* Linear search for index to match. */
	for (i = 0; i < size - 1; i++) {
		if (eng_val <= pairs[i].val)
			return i;

		if (eng_val < pairs[i+1].val) {
			if (round_up)
				return i + 1;
			else
				return i;
		}
	}

	return i;
}

/**
 * Read register from accelerometer.
 */
static int raw_read8(const int addr, const int reg, int *data_ptr)
{
	return i2c_read8(I2C_PORT_ACCEL, addr, reg, data_ptr);
}

/**
 * Write register from accelerometer.
 */
static int raw_write8(const int addr, const int reg, int data)
{
	return i2c_write8(I2C_PORT_ACCEL, addr, reg, data);
}

int accel_set_range(const enum accel_id id, const int range, const int rnd)
{
	int ret, index, ctrl_reg6;

	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	/* Find index for interface pair matching the specified range. */
	index = find_param_index(range, rnd, g_ranges, ARRAY_SIZE(g_ranges));

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(&lsm6ds0_mutex[id]);

	ret = raw_read8(lsm6ds0_addr[id], LSM6DS0_CTRL_REG6_XL, &ctrl_reg6);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	ctrl_reg6 = (ctrl_reg6 & ~LSM6DS0_GSEL_MASK) | g_ranges[index].reg;
	ret = raw_write8(lsm6ds0_addr[id], LSM6DS0_CTRL_REG6_XL, ctrl_reg6);

accel_cleanup:
	/* Unlock accel resource and save new range if written successfully. */
	mutex_unlock(&lsm6ds0_mutex[id]);
	if (ret == EC_SUCCESS)
		sensor_g_range[id] = index;

	return EC_SUCCESS;
}

int accel_get_range(const enum accel_id id, int * const range)
{
	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	*range = g_ranges[sensor_g_range[id]].val;
	return EC_SUCCESS;
}

int accel_set_resolution(const enum accel_id id, const int res, const int rnd)
{
	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	/* Only one resolution, LSM6DS0_RESOLUTION, so nothing to do. */
	return EC_SUCCESS;
}

int accel_get_resolution(const enum accel_id id, int * const res)
{
	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	*res = LSM6DS0_RESOLUTION;
	return EC_SUCCESS;
}

int accel_set_datarate(const enum accel_id id, const int rate, const int rnd)
{
	int ret, index, ctrl_reg6;
	const struct accel_param_pair *la_odr_tbl;

	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	la_odr_tbl = get_la_odr_tbl(id);

	/* Find index for interface pair matching the specified range. */
	index = find_param_index(rate, rnd, la_odr_tbl, LA_ODR_TBL_SIZE);

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(&lsm6ds0_mutex[id]);

	ret = raw_read8(lsm6ds0_addr[id], LSM6DS0_CTRL_REG6_XL, &ctrl_reg6);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	ctrl_reg6 = (ctrl_reg6 & ~LSM6DS0_ODR_MASK) | la_odr_tbl[index].reg;
	ret = raw_write8(lsm6ds0_addr[id], LSM6DS0_CTRL_REG6_XL, ctrl_reg6);

accel_cleanup:
	/* Unlock accel resource and save new ODR if written successfully. */
	mutex_unlock(&lsm6ds0_mutex[id]);
	if (ret == EC_SUCCESS)
		la_sensor_datarate[id] = index;

	return EC_SUCCESS;
}

int accel_get_datarate(const enum accel_id id, int * const rate)
{
	const struct accel_param_pair *la_odr_tbl;

	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	la_odr_tbl = get_la_odr_tbl(id);

	*rate = la_odr_tbl[la_sensor_datarate[id]].val;
	return EC_SUCCESS;
}

#ifdef CONFIG_ACCEL_INTERRUPTS
int accel_set_interrupt(const enum accel_id id, unsigned int threshold)
{
	/* Currently unsupported. */
	return EC_ERROR_UNKNOWN;
}
#endif

int accel_read(const enum accel_id id, int * const x_acc, int * const y_acc,
		int * const z_acc)
{
	uint8_t acc[6];
	uint8_t reg = LSM6DS0_OUT_X_L_XL;
	int ret, multiplier;

	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	/* Read 6 bytes starting at LSM6DS0_OUT_X_L_XL. */
	mutex_lock(&lsm6ds0_mutex[id]);
	i2c_lock(I2C_PORT_ACCEL, 1);
	ret = i2c_xfer(I2C_PORT_ACCEL, lsm6ds0_addr[id], &reg, 1, acc, 6,
			I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_ACCEL, 0);
	mutex_unlock(&lsm6ds0_mutex[id]);

	if (ret != EC_SUCCESS)
		return ret;

	/* Determine multiplier based on stored range. */
	switch (g_ranges[sensor_g_range[id]].reg) {
	case LSM6DS0_GSEL_2G:
		multiplier = 1;
		break;
	case LSM6DS0_GSEL_4G:
		multiplier = 2;
		break;
	case LSM6DS0_GSEL_8G:
		multiplier = 4;
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}

	/*
	 * Convert data to signed 12-bit value. Note order of registers:
	 *
	 * acc[0] = LSM6DS0_OUT_X_L_XL
	 * acc[1] = LSM6DS0_OUT_X_H_XL
	 * acc[2] = LSM6DS0_OUT_Y_L_XL
	 * acc[3] = LSM6DS0_OUT_Y_H_XL
	 * acc[4] = LSM6DS0_OUT_Z_L_XL
	 * acc[5] = LSM6DS0_OUT_Z_H_XL
	 */
	*x_acc = multiplier * ((int16_t)(acc[1] << 8 | acc[0])) >> 4;
	*y_acc = multiplier * ((int16_t)(acc[3] << 8 | acc[2])) >> 4;
	*z_acc = multiplier * ((int16_t)(acc[5] << 8 | acc[4])) >> 4;

	return EC_SUCCESS;
}

int accel_init(const enum accel_id id)
{
	int ret, ctrl_reg6;
	const struct accel_param_pair *la_odr_tbl;

	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	la_odr_tbl = gyro_off_odr;
	set_la_odr_tbl(id, la_odr_tbl);

	mutex_lock(&lsm6ds0_mutex[id]);

	/*
	 * This sensor can be powered through an EC reboot, so the state of
	 * the sensor is unknown here. Initiate software reset to restore
	 * sensor to default.
	 */
	ret = raw_write8(lsm6ds0_addr[id], LSM6DS0_CTRL_REG8, 1);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	/* Set LA ODR and range. */
	ctrl_reg6 = la_odr_tbl[la_sensor_datarate[id]].reg |
			g_ranges[sensor_g_range[id]].reg;

	ret = raw_write8(lsm6ds0_addr[id], LSM6DS0_CTRL_REG6_XL, ctrl_reg6);

accel_cleanup:
	mutex_unlock(&lsm6ds0_mutex[id]);
	return ret;
}


int gyro_set_range(const enum accel_id id, const int range, const int rnd)
{
	int ret, index, ctrl_reg1;

	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	/* Find index for interface pair matching the specified range. */
	index = find_param_index(range, rnd,
		dps_ranges, ARRAY_SIZE(dps_ranges));

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(&lsm6ds0_mutex[id]);

	ret = raw_read8(lsm6ds0_addr[id], LSM6DS0_CTRL_REG1_G, &ctrl_reg1);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	ctrl_reg1 = (ctrl_reg1 & ~LSM6DS0_DPS_SEL_MASK) | dps_ranges[index].reg;
	ret = raw_write8(lsm6ds0_addr[id], LSM6DS0_CTRL_REG1_G, ctrl_reg1);

accel_cleanup:
	/* Unlock accel resource and save new range if written successfully. */
	mutex_unlock(&lsm6ds0_mutex[id]);
	if (ret == EC_SUCCESS)
		sensor_dps_range[id] = index;

	return EC_SUCCESS;
}
int gyro_get_range(const enum accel_id id, int * const range)
{
	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	*range = dps_ranges[sensor_dps_range[id]].val;
	return EC_SUCCESS;
}
int gyro_set_datarate(const enum accel_id id, const int rate, const int rnd)
{
	int ret, index, ctrl_reg1;
	const struct accel_param_pair *la_odr_tbl;

	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	la_odr_tbl = get_la_odr_tbl(id);

	/* Find index for interface pair matching the specified range. */
	index = find_param_index(rate, rnd, la_odr_tbl, LA_ODR_TBL_SIZE);

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(&lsm6ds0_mutex[id]);

	ret = raw_read8(lsm6ds0_addr[id], LSM6DS0_CTRL_REG1_G, &ctrl_reg1);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	ctrl_reg1 = (ctrl_reg1 & ~LSM6DS0_ODR_MASK) | la_odr_tbl[index].reg;
	ret = raw_write8(lsm6ds0_addr[id], LSM6DS0_CTRL_REG1_G, ctrl_reg1);

accel_cleanup:
	/* Unlock accel resource and save new ODR if written successfully. */
	mutex_unlock(&lsm6ds0_mutex[id]);
	if (ret == EC_SUCCESS)
		gyro_sensor_datarate[id] = index;

	return EC_SUCCESS;
}

int gyro_get_datarate(const enum accel_id id, int * const rate)
{
	const struct accel_param_pair *la_odr_tbl;

	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	la_odr_tbl = get_la_odr_tbl(id);

	*rate = la_odr_tbl[gyro_sensor_datarate[id]].val;
	return EC_SUCCESS;
}

int gyro_read(const enum accel_id id, int * const x_gyro, int * const y_gyro,
		int * const z_gyro)
{
	uint8_t gyro[6];
	uint8_t reg = LSM6DS0_OUT_X_L_G;
	int ret;

	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	/* Read 6 bytes starting at LSM6DS0_OUT_X_L_G. */
	mutex_lock(&lsm6ds0_mutex[id]);
	i2c_lock(I2C_PORT_ACCEL, 1);
	ret = i2c_xfer(I2C_PORT_ACCEL, lsm6ds0_addr[id], &reg, 1, gyro, 6,
			I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_ACCEL, 0);
	mutex_unlock(&lsm6ds0_mutex[id]);

	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * Convert data to signed 16-bit value. Note order of registers:
	 *
	 * gyro[0] = LSM6DS0_OUT_X_L_G
	 * gyro[1] = LSM6DS0_OUT_X_H_G
	 * gyro[2] = LSM6DS0_OUT_Y_L_G
	 * gyro[3] = LSM6DS0_OUT_Y_H_G
	 * gyro[4] = LSM6DS0_OUT_Z_L_G
	 * gyro[5] = LSM6DS0_OUT_Z_H_G
	 */
	*x_gyro = ((int16_t)(gyro[1] << 8 | gyro[0]));
	*y_gyro = ((int16_t)(gyro[3] << 8 | gyro[2]));
	*z_gyro = ((int16_t)(gyro[5] << 8 | gyro[4]));

	return EC_SUCCESS;
}

int gyro_init(const enum accel_id id)
{
	int ret, ctrl_reg1;
	const struct accel_param_pair *la_odr_tbl;

	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	la_odr_tbl = gyro_on_odr;
	set_la_odr_tbl(id, la_odr_tbl);

	/* Check for valid id. */
	if (id < 0 || id >= ACCEL_COUNT)
		return EC_ERROR_INVAL;

	mutex_lock(&lsm6ds0_mutex[id]);

	/* Set Gyro ODR and range. */
	ctrl_reg1 = la_odr_tbl[gyro_sensor_datarate[id]].reg |
			dps_ranges[sensor_dps_range[id]].reg;

	ret = raw_write8(lsm6ds0_addr[id], LSM6DS0_CTRL_REG1_G, ctrl_reg1);

	mutex_unlock(&lsm6ds0_mutex[id]);
	return ret;
}

