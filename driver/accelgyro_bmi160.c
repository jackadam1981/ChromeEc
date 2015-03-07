/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * BMI160/BMC50 accelerometer and gyro module for Chrome EC
 * 3D digital accelerometer & 3D digital gyroscope
 */

#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/accelgyro_bmi160.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

/*
 * Struct for pairing an engineering value with the register value for a
 * parameter.
 */
struct accel_param_pair {
	int val; /* Value in engineering units. */
	int reg_val; /* Corresponding register value. */
};

/* List of range values in +/-G's and their associated register values. */
static const struct accel_param_pair g_ranges[] = {
	{2, BMI160_GSEL_2G},
	{4, BMI160_GSEL_4G},
	{8, BMI160_GSEL_8G},
	{16, BMI160_GSEL_8G}
};

/*
 * List of angular rate range values in +/-dps's
 * and their associated register values.
 */
const struct accel_param_pair dps_ranges[] = {
	{125, BMI160_DPS_SEL_125},
	{250, BMI160_DPS_SEL_250},
	{500, BMI160_DPS_SEL_500},
	{1000, BMI160_DPS_SEL_1000},
	{2000, BMI160_DPS_SEL_2000}
};

static inline const struct accel_param_pair *get_range_table(
		enum motionsensor_type type, int *psize)
{
	if (MOTIONSENSE_TYPE_ACCEL == type) {
		if (psize)
			*psize = ARRAY_SIZE(g_ranges);
		return g_ranges;
	} else {
		if (psize)
			*psize = ARRAY_SIZE(dps_ranges);
		return dps_ranges;
	}
}

/* List of ODR (gyro off) values in mHz and their associated register values.*/
/* The table is calculated from: value = 100 / (1 << (8 - reg_val)) */
const struct accel_param_pair accel_mag_odr[] = {
	{ 0, BMI160_ACCEL_ODR_0HZ},
	{ 780, BMI160_ACCEL_ODR_0_78HZ},
	{ 1560, BMI160_ACCEL_ODR_1_56HZ},
	{ 3120, BMI160_ACCEL_ODR_3_12HZ},
	{ 6250, BMI160_ACCEL_ODR_6_25HZ},
	{ 12500, BMI160_ACCEL_ODR_12_5HZ},
	{ 25000, BMI160_ACCEL_ODR_25HZ},
	{ 50000, BMI160_ACCEL_ODR_50HZ},
	{ 100000, BMI160_ACCEL_ODR_100HZ},
	{ 200000, BMI160_ACCEL_ODR_200HZ},
	{ 400000, BMI160_ACCEL_ODR_400HZ},
	{ 800000, BMI160_ACCEL_ODR_800HZ},
	{ 1600000, BMI160_ACCEL_ODR_1600HZ},
};

/* List of ODR (gyro on) values in mHz and their associated register values. */
/* The table is calculated from: value = 100 / (1 << (7 - reg_val)) */
const struct accel_param_pair gyro_odr[] = {
	{ 0, BMI160_GYRO_ODR_0HZ},
	{ 25000, BMI160_GYRO_ODR_25HZ},
	{ 50000, BMI160_GYRO_ODR_50HZ},
	{ 100000, BMI160_GYRO_ODR_100HZ},
	{ 200000, BMI160_GYRO_ODR_200HZ},
	{ 400000, BMI160_GYRO_ODR_400HZ},
	{ 800000, BMI160_GYRO_ODR_800HZ},
	{ 1600000, BMI160_GYRO_ODR_1600HZ},
	{ 3200000, BMI160_GYRO_ODR_3200HZ},
};

static inline const struct accel_param_pair *get_odr_table(
		enum motionsensor_type type, int *psize)
{
	if (MOTIONSENSE_TYPE_GYRO == type) {
		if (psize)
			*psize = ARRAY_SIZE(gyro_odr);
		return gyro_odr;
	} else {
		if (psize)
			*psize = ARRAY_SIZE(accel_mag_odr);
		return accel_mag_odr;
	}
}

static inline int get_xyz_reg(enum motionsensor_type type)
{
	switch (type) {
	case MOTIONSENSE_TYPE_ACCEL:
		return BMI160_ACC_X_L_G;
	case MOTIONSENSE_TYPE_GYRO:
		return BMI160_GYR_X_L_G;
	case MOTIONSENSE_TYPE_MAG:
		return BMI160_MAG_X_L_G;
	default:
		return -1;
	}
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

static int set_range(const struct motion_sensor_t *s,
				int range,
				int rnd)
{
	int ret, range_tbl_size;
	uint8_t reg_val, ctrl_reg;
	const struct accel_param_pair *ranges;

	ctrl_reg = BMI160_RANGE_REG(s->type);
	ranges = get_range_table(s->type, &range_tbl_size);
	reg_val = get_reg_val(range, rnd, ranges, range_tbl_size);

	ret = raw_write8(s->i2c_addr, ctrl_reg, reg_val);
	return ret;
}

static int get_range(const struct motion_sensor_t *s,
				int *range)
{
	int ret, ctrl_val, range_tbl_size;
	uint8_t ctrl_reg;
	const struct accel_param_pair *ranges;
	ranges = get_range_table(s->type, &range_tbl_size);
	ctrl_reg = BMI160_RANGE_REG(s->type);
	ret = raw_read8(s->i2c_addr, ctrl_reg, &ctrl_val);
	*range = get_engineering_val(ctrl_val, ranges, range_tbl_size);
	return ret;
}

static int set_resolution(const struct motion_sensor_t *s,
				int res,
				int rnd)
{
	/* Only one resolution, BMI160_RESOLUTION, so nothing to do. */
	return EC_SUCCESS;
}

static int get_resolution(const struct motion_sensor_t *s,
				int *res)
{
	*res = BMI160_RESOLUTION;
	return EC_SUCCESS;
}

static int set_data_rate(const struct motion_sensor_t *s,
				int rate,
				int rnd)
{
	int ret, val, odr_tbl_size;
	uint8_t ctrl_reg, reg_val;
	const struct accel_param_pair *data_rates;

	ctrl_reg = BMI160_RANGE_REG(s->type);
	data_rates = get_odr_table(s->type, &odr_tbl_size);
	reg_val = get_reg_val(rate, rnd, data_rates, odr_tbl_size);

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(s->mutex);

	ret = raw_read8(s->i2c_addr, ctrl_reg, &val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	val = (val & ~BMI160_ODR_MASK) | reg_val;
	ret = raw_write8(s->i2c_addr, ctrl_reg, val);


accel_cleanup:
	mutex_unlock(s->mutex);
	return EC_SUCCESS;
}

static int get_data_rate(const struct motion_sensor_t *s,
				int *rate)
{
	int ret, ctrl_val, odr_tbl_size;
	uint8_t ctrl_reg;
	const struct accel_param_pair *data_rates;
	ctrl_reg = BMI160_RANGE_REG(s->type);

	ret = raw_read8(s->i2c_addr, ctrl_reg, &ctrl_val);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	data_rates = get_odr_table(s->type, &odr_tbl_size);
	*rate = get_engineering_val(ctrl_val & BMI160_ODR_MASK,
			data_rates, odr_tbl_size);
	return EC_SUCCESS;
}

#ifdef CONFIG_ACCEL_INTERRUPTS
static int set_interrupt(const struct motion_sensor_t *s,
			       unsigned int threshold)
{
	/* Currently unsupported. */
	return EC_ERROR_UNKNOWN;
}
#endif

static int is_data_ready(const struct motion_sensor_t *s, int *ready)
{
	int ret, tmp;

	ret = raw_read8(s->i2c_addr, BMI160_STATUS, &tmp);

	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RS Error]", s->name, s->type);
		return ret;
	}

	*ready = tmp & BMI160_DRDY_MASK(s->type);
	return EC_SUCCESS;
}

static int read(const struct motion_sensor_t *s, vector_3_t v)
{
	uint8_t data[6];
	uint8_t xyz_reg;
	int ret, tmp = 0, range = 0;

	ret = is_data_ready(s, &tmp);
	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * If sensor data is not ready, return the previous read data.
	 * Note: return success so that motion senor task can read again
	 * to get the latest updated sensor data quickly.
	 */
	if (!tmp) {
		v[0] = s->raw_xyz[0];
		v[1] = s->raw_xyz[1];
		v[2] = s->raw_xyz[2];
		return EC_SUCCESS;
	}

	xyz_reg = get_xyz_reg(s->type);

	/* Read 6 bytes starting at xyz_reg */
	i2c_lock(I2C_PORT_ACCEL, 1);
	ret = i2c_xfer(I2C_PORT_ACCEL, s->i2c_addr,
			&xyz_reg, 1, data, 6, I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_ACCEL, 0);

	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RD XYZ Error %d]",
			s->name, s->type, ret);
		return ret;
	}

	v[0] = ((int16_t)((data[1] << 8) | data[0]));
	v[1] = ((int16_t)((data[3] << 8) | data[2]));
	v[2] = ((int16_t)((data[5] << 8) | data[4]));

	ret = get_range(s, &range);
	if (ret)
		return EC_ERROR_UNKNOWN;

	v[0] *= range;
	v[1] *= range;
	v[2] *= range;

	/* normalize the accel scale: 1G = 1024 */
	if (MOTIONSENSE_TYPE_ACCEL == s->type) {
		v[0] >>= 5;
		v[1] >>= 5;
		v[2] >>= 5;
	} else {
		v[0] >>= 8;
		v[1] >>= 8;
		v[2] >>= 8;
	}

	return EC_SUCCESS;
}

static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp;

	ret = raw_read8(s->i2c_addr, BMI160_CHIP_ID, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;

	if (tmp != BMI160_CHIP_ID_MAJOR)
		return EC_ERROR_ACCESS_DENIED;


	/* To avoid gyro wakeup */
	raw_write8(s->i2c_addr, BMI160_PMU_TRIGGER, 0);

	raw_write8(s->i2c_addr, BMI160_CMD_REG,
			BMI150_CMD_MODE_NORMAL(s->type));
	msleep(30);
	/* set acc bandwith average 4 */
	/* set gyr bandwith normal */

	/* 100Hz */
	set_data_rate(s, 100000, 0);

	/* Fifo setup is done elsewhere */

	CPRINTF("[%T %s: MS Done Init type:0x%X range:%d odr:%d]\n",
			s->name, s->type, s->range, s->odr);
	return ret;
}

const struct accelgyro_drv bmi160_drv = {
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
