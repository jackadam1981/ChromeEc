/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * L3GD20 gyro module for Chrome EC 3D digital gyroscope.
 */

#include "common.h"
#include "console.h"
#include "driver/gyro_l3gd20h.h"
#include "timer.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

/*
 * Struct for pairing an engineering value with the register value for a
 * parameter.
 */
struct gyro_param_pair {
	int val; /* Value in engineering units. */
	int reg_val; /* Corresponding register value. */
};

/*
 * List of angular rate range values in +/-dps's
 * and their associated register values.
 */
const struct gyro_param_pair dps_ranges[] = {
	{245, L3GD20_DPS_SEL_245},
	{500, L3GD20_DPS_SEL_500},
	{2000, L3GD20_DPS_SEL_2000_0},
	{2000, L3GD20_DPS_SEL_2000_1},
};

static inline const struct gyro_param_pair *get_range_table(
		enum sensor_type_t type, int *psize)
{
	if (psize)
		*psize = ARRAY_SIZE(dps_ranges);
	return dps_ranges;
}

/* List of ODR values in mHz and their associated register values. */
const struct gyro_param_pair gyro_odr[] = {
	{0,      L3GD20_ODR_PD | L3GD20_LOW_ODR_MASK},
	{12500,  L3GD20_ODR_12_5HZ | L3GD20_ODR_PD_MASK | L3GD20_LOW_ODR_MASK},
	{25000,  L3GD20_ODR_25HZ | L3GD20_ODR_PD_MASK | L3GD20_LOW_ODR_MASK},
	{50000,  L3GD20_ODR_50HZ_0 | L3GD20_ODR_PD_MASK | L3GD20_LOW_ODR_MASK},
	{50000,  L3GD20_ODR_50HZ_1 | L3GD20_ODR_PD_MASK | L3GD20_LOW_ODR_MASK},
	{100000, L3GD20_ODR_100HZ | L3GD20_ODR_PD_MASK},
	{200000, L3GD20_ODR_200HZ | L3GD20_ODR_PD_MASK},
	{400000, L3GD20_ODR_400HZ | L3GD20_ODR_PD_MASK},
	{800000, L3GD20_ODR_800HZ | L3GD20_ODR_PD_MASK},
};

static inline const struct gyro_param_pair *get_odr_table(
		enum sensor_type_t type, int *psize)
{
	if (psize)
		*psize = ARRAY_SIZE(gyro_odr);
	return gyro_odr;
}

static inline int get_ctrl_reg(enum sensor_type_t type)
{
	return L3GD20_CTRL_REG1;
}

static inline int get_xyz_reg(enum sensor_type_t type)
{
	/*
	* We need to set msb of the X_L register in order to read
	* multiple bytes. See the datasheet section 5.1.1 for the details.
	*/
	return L3GD20_OUT_X_L | (1 << 7);
}

/**
 * @return reg value that matches the given engineering value passed in.
 * The round_up flag is used to specify whether to round up or down.
 * Note, this function always returns a valid reg value. If the request is
 * outside the range of values, it returns the closest valid reg value.
 */
static int get_reg_val(const int eng_val, const int round_up,
		const struct gyro_param_pair *pairs, const int size)
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
		const struct gyro_param_pair *pairs, const int size)
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
	return i2c_read8(I2C_PORT_GYRO, addr, reg, data_ptr);
}

/**
 * Write register from accelerometer.
 */
static inline int raw_write8(const int addr, const int reg, int data)
{
	return i2c_write8(I2C_PORT_GYRO, addr, reg, data);
}

static int set_range(const struct motion_sensor_t *s,
				int range,
				int rnd)
{
	int ret, ctrl_val, range_tbl_size;
	uint8_t reg_val;
	const struct gyro_param_pair *ranges;

	ranges = get_range_table(s->type, &range_tbl_size);
	reg_val = get_reg_val(range, rnd, ranges, range_tbl_size);

	mutex_lock(s->mutex);

	ret = raw_read8(s->i2c_addr, L3GD20_CTRL_REG4, &ctrl_val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	ctrl_val = (ctrl_val & ~L3GD20_RANGE_MASK) | reg_val;
	ret = raw_write8(s->i2c_addr, L3GD20_CTRL_REG4, ctrl_val);

accel_cleanup:
	mutex_unlock(s->mutex);
	return ret;
}

static int get_range(const struct motion_sensor_t *s,
				int *range)
{
	int ret, ctrl_val, range_tbl_size;

	const struct gyro_param_pair *ranges;
	ranges = get_range_table(s->type, &range_tbl_size);

	ret = raw_read8(s->i2c_addr, L3GD20_CTRL_REG4, &ctrl_val);
	*range = get_engineering_val(ctrl_val & L3GD20_RANGE_MASK,
		ranges, range_tbl_size);
	return ret;
}

static int set_resolution(const struct motion_sensor_t *s,
				int res,
				int rnd)
{
	/* Only one resolution, L3GD20_RESOLUTION, so nothing to do. */
	return EC_SUCCESS;
}

static int get_resolution(const struct motion_sensor_t *s,
				int *res)
{
	*res = L3GD20_RESOLUTION;
	return EC_SUCCESS;
}

static int set_data_rate(const struct motion_sensor_t *s,
				int rate,
				int rnd)
{
	int ret, val, odr_tbl_size;
	uint8_t ctrl_reg, reg_val;
	const struct gyro_param_pair *data_rates;

	ctrl_reg = get_ctrl_reg(s->type);
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

	val = (val & ~(L3GD20_ODR_MASK | L3GD20_ODR_PD_MASK)) |
		(reg_val & ~L3GD20_LOW_ODR_MASK);
	ret = raw_write8(s->i2c_addr, ctrl_reg, val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	ret = raw_read8(s->i2c_addr, L3GD20_LOW_ODR, &val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;
	/* We need to clear low_ODR bit for higher data rates */
	if (reg_val & L3GD20_LOW_ODR_MASK)
		val |= 1;
	else
		val &= ~1;

	ret = raw_write8(s->i2c_addr, L3GD20_LOW_ODR, val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	/* CTRL_REG5 24h
	 * [7] low-power mode = 0;
	 * [6] fifo disabled = 0;
	 * [5] Stop on fth = 0;
	 * [4] High pass filter enable = 1;
	 * [3:2] int1_sel = 0;
	 * [1:0] out_sel = 1;
	 */
	ret = raw_read8(s->i2c_addr, L3GD20_CTRL_REG5, &val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	val |= (1 << 4); /* high-pass filter enabled */
	val |= (1 << 0); /* data in data reg are high-pass filtered */
	ret = raw_write8(s->i2c_addr, L3GD20_CTRL_REG5, val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	ret = raw_read8(s->i2c_addr, L3GD20_CTRL_REG2, &val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	val &= 0xf0;
	val |= 0x04; /* Table 13. high-pass filter cutoff table */
	ret = raw_write8(s->i2c_addr, L3GD20_CTRL_REG2, val);

accel_cleanup:
	mutex_unlock(s->mutex);
	return ret;
}

static int get_data_rate(const struct motion_sensor_t *s,
				int *rate)
{
	int ret, ctrl_val, odr_tbl_size, low_odr;
	uint8_t ctrl_reg;
	const struct gyro_param_pair *data_rates;
	ctrl_reg = get_ctrl_reg(s->type);

	ret = raw_read8(s->i2c_addr, ctrl_reg, &ctrl_val);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	ret = raw_read8(s->i2c_addr, L3GD20_LOW_ODR, &low_odr);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	data_rates = get_odr_table(s->type, &odr_tbl_size);
	*rate = get_engineering_val((ctrl_val &
		(L3GD20_ODR_MASK | L3GD20_ODR_PD_MASK))
		| (low_odr & L3GD20_LOW_ODR_MASK),
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
	int ret, tmp = 0;

	ret = raw_read8(s->i2c_addr, L3GD20_STATUS_REG, &tmp);
	if (ret != EC_SUCCESS) {
		*ready = 0;
		CPRINTF("[%T %s type:0x%X RS Error]", s->name, s->type);
		return ret;
	}

	*ready = (tmp & L3GD20_STS_ZYXDA_MASK) ? 1 : 0;
	return EC_SUCCESS;
}

static int read(const struct motion_sensor_t *s, vector_3_t v)
{
	uint8_t data[6];
	uint8_t xyz_reg;
	int ret, data_avail = 0, range = 0;

	ret = is_data_ready(s, &data_avail);
	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * If sensor data is not ready, return the previous read data.
	 * Note: return success so that motion senor task can read again
	 * to get the latest updated sensor data quickly.
	 */
	if (!data_avail) {
		v[0] = s->xyz[0];
		v[1] = s->xyz[1];
		v[2] = s->xyz[2];
		return EC_SUCCESS;
	}

	xyz_reg = get_xyz_reg(s->type);

	/* Read 6 bytes starting at xyz_reg */
	i2c_lock(I2C_PORT_ACCEL, 1);
	ret = i2c_xfer(I2C_PORT_ACCEL, s->i2c_addr,
			&xyz_reg, 1, data, 6, I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_ACCEL, 0);

	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RD XYZ Error]",
			s->name, s->type);
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

	v[0] /= 256;
	v[1] /= 256;
	v[2] /= 256;

	return EC_SUCCESS;
}

static int gyro_read(int argc , char *argv[])
{
	vector_3_t v;
	int rv;
	int i = 99;

	v[0] = v[1] = v[2] = 0;

	do {
		rv = read(&motion_sensors[0], v);
		if (rv != EC_SUCCESS)
			break;
		CPRINTF("l3gd20_drv: in %s | x: %d, y: %d, z: %d\n",
			__func__, v[0], v[1], v[2]);
		msleep(100);
	} while (i--);

	return rv;
}
DECLARE_CONSOLE_COMMAND(gyroread, gyro_read, NULL, NULL, NULL);

static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp;
	int i;
	struct motion_sensor_t *sensor;
	for (i = 0; i < motion_sensor_count; ++i) {
		sensor = &motion_sensors[i];
		sensor->state = SENSOR_NOT_INITIALIZED;

		sensor->odr = sensor->default_odr;
		sensor->range = sensor->default_range;
	}
	ret = raw_read8(s->i2c_addr, L3GD20_WHO_AM_I_REG, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;

	if (tmp != L3GD20_WHO_AM_I)
		return EC_ERROR_ACCESS_DENIED;

	/* All axes are enabled */
	ret = raw_write8(s->i2c_addr, L3GD20_CTRL_REG1, 0x0f);
	if (ret)
		return EC_ERROR_UNKNOWN;

	mutex_lock(s->mutex);
	ret = raw_read8(s->i2c_addr, L3GD20_CTRL_REG4, &tmp);
	if (ret) {
		mutex_unlock(s->mutex);
		return EC_ERROR_UNKNOWN;
	}
	tmp |= L3GD20_BDU_ENABLE;
	ret = raw_write8(s->i2c_addr, L3GD20_CTRL_REG4, tmp);
	mutex_unlock(s->mutex);

	if (ret)
		return EC_ERROR_UNKNOWN;

	/* Config GYRO Range */
	ret = set_range(s, s->range, 1);
	if (ret)
		return EC_ERROR_UNKNOWN;

	/* Config GYRO ODR */
	ret = set_data_rate(s, s->odr, 1);
	if (ret)
		return EC_ERROR_UNKNOWN;

	CPRINTF("[%T %s: MS Done Init type:0x%X range:%d odr:%d]\n",
			s->name, s->type, s->range, s->odr);
	return ret;
}

static int gyro_init(int argc, char *argv[])
{
	return init(&motion_sensors[0]);
}
DECLARE_CONSOLE_COMMAND(gyroinit, gyro_init, NULL, NULL, NULL);

static int gyro_test(int argc, char *argv[])
{
	int ret, range, rnd, rate, iter;
	for (rnd = 0; rnd < 2; rnd++) {
		CPRINTF("%s round: %d\n", __func__, rnd);
		for (iter = 0; iter < ARRAY_SIZE(dps_ranges); iter++) {
			range = dps_ranges[iter].val;
			ret = set_range(&motion_sensors[0], range, rnd);
			if (ret != EC_SUCCESS)
				break;
			CPRINTF("%s set_range: %d\n", __func__, range);
			ret = get_range(&motion_sensors[0], &range);
			if (ret != EC_SUCCESS)
				break;
			CPRINTF("%s get_range: %d\n", __func__, range);
		}
	}

	for (rnd = 0; rnd < 2; rnd++) {
		CPRINTF("%s round: %d\n", __func__, rnd);
		for (iter = 0; iter < ARRAY_SIZE(gyro_odr); iter++) {
			rate = gyro_odr[iter].val;
			ret = set_data_rate(&motion_sensors[0], rate, rnd);
			if (ret != EC_SUCCESS)
				break;
			CPRINTF("%s set_data_rate: %d\n", __func__, rate);
			ret = get_data_rate(&motion_sensors[0], &rate);
			if (ret != EC_SUCCESS)
				break;
			CPRINTF("%s get_data_rate: %d\n", __func__, rate);
		}
	}

	return ret;
}
DECLARE_CONSOLE_COMMAND(gyrotest, gyro_test, NULL, NULL, NULL);

const struct accelgyro_drv l3gd20_drv = {
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
