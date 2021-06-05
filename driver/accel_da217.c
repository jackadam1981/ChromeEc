/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Accelerometer driver for Chrome EC
 *
 * Supported: MIR3DA
 */

#include "accelgyro.h"
#include "common.h"
#include "accel_da217.h"
#include "console.h"
#include "i2c.h"
#include "math_util.h"
#include "spi.h"
#include "task.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

/* Number of times to attempt to enable sensor before giving up. */
#define SENSOR_ENABLE_ATTEMPTS 5


/**
 * Read register from accelerometer.
 */
static inline int raw_read8(const int port, const uint16_t i2c_addr_flags,
			    const int reg, int *data_ptr)
{
	return i2c_read8(port, i2c_addr_flags, reg, data_ptr);
}

/**
 * Write register from accelerometer.
 */
static inline int raw_write8(const int port, const uint16_t i2c_addr_flags,
			     const int reg, int data)
{
	return i2c_write8(port, i2c_addr_flags, reg, data);
}

static int set_range(struct motion_sensor_t *s, int range, int rnd)
{
	int ret,  range_val, reg_val, range_reg_val;

	range_val = MIR3DA_RANGE_TO_REG(range);
	if ((MIR3DA_RANGE_TO_REG(range_val) < range) && rnd)
		range_val = MIR3DA_RANGE_TO_REG(range * 2);

	mutex_lock(s->mutex);

	/* Determine the new value of control reg and attempt to write it. */
	ret = raw_read8(s->port, s->i2c_spi_addr_flags,
			MIR3DA_RESOLUTION_RANGE, &range_reg_val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}
	reg_val = (range_reg_val & ~MIR3DA_RANGE_MSK) | range_val;
	ret = raw_write8(s->port, s->i2c_spi_addr_flags,
			 MIR3DA_RESOLUTION_RANGE, reg_val);

	/* If successfully written, then save the range. */
	if (ret == EC_SUCCESS)
		s->current_range = MIR3DA_REG_TO_RANGE(range_val);

	mutex_unlock(s->mutex);

	return ret;
}

static int get_resolution(const struct motion_sensor_t *s)
{
	return MIR3DA_RESOLUTION;
}

static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret, odr_val, odr_reg_val, reg_val;
	struct accelgyro_saved_data_t *data = s->drv_data;

	if (rate == 0) {
		mutex_lock(s->mutex);
		/* power off. */
		reg_val = MIR3DA_POWER_OFF;
		ret = raw_write8(s->port, s->i2c_spi_addr_flags,
				 MIR3DA_MODE_BW, reg_val);
		mutex_unlock(s->mutex);
		return ret;
	}

	odr_val = MIR3DA_ODR_TO_REG(rate);
	if ((MIR3DA_REG_TO_BW(odr_val) < rate) && rnd)
		odr_val = MIR3DA_ODR_TO_REG(rate * 2);

	mutex_lock(s->mutex);

	/* Determine the new value of control reg and attempt to write it. */
	ret = raw_read8(s->port, s->i2c_spi_addr_flags,
			MIR3DA_ODR_AXIS, &odr_reg_val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}
	reg_val = (odr_reg_val & ~MIR3DA_ODR_MSK) | odr_val;
	/* Set output data rate. */
	ret = raw_write8(s->port, s->i2c_spi_addr_flags,
			 MIR3DA_ODR_AXIS, reg_val);

	/* If successfully written, then save the new data rate. */
	if (ret == EC_SUCCESS)
		data->odr = MIR3DA_REG_TO_BW(odr_val);

	/* power on. */
	reg_val = MIR3DA_POWER_ON;
	ret = raw_write8(s->port, s->i2c_spi_addr_flags,
			 MIR3DA_MODE_BW, reg_val);

	mutex_unlock(s->mutex);
	return ret;
}

static int get_data_rate(const struct motion_sensor_t *s)
{
	struct accelgyro_saved_data_t *data = s->drv_data;

	return data->odr;
}

static int set_offset(const struct motion_sensor_t *s, const int16_t *offset,
		      int16_t temp)
{
	struct mir3da_accel_data *data = s->drv_data;

	data->offset[X] = offset[X];
	data->offset[Y] = offset[Y];
	data->offset[Z] = offset[Z];

	return EC_SUCCESS;
}

static int get_offset(const struct motion_sensor_t *s, int16_t *offset,
		      int16_t *temp)
{
	struct mir3da_accel_data *data = s->drv_data;

	offset[X] = data->offset[X];
	offset[Y] = data->offset[Y];
	offset[Z] = data->offset[Z];
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;

	return EC_SUCCESS;
}

static int read(const struct motion_sensor_t *s, intv3_t v)
{
	uint8_t acc[6];
	int ret, i;

	/* Read 6 bytes starting at X_AXIS_LSB. */
	mutex_lock(s->mutex);
	ret = i2c_read_block(s->port, s->i2c_spi_addr_flags,
			     MIR3DA_ACC_X_LSB, acc, 6);
	mutex_unlock(s->mutex);

	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * Convert acceleration to a signed 16-bit number. Note, based on
	 * the order of the registers:
	 *
	 * acc[0] = X_AXIS_LSB -> bit 7~4 for value, bit 0 for new data bit
	 * acc[1] = X_AXIS_MSB
	 * acc[2] = Y_AXIS_LSB -> bit 7~4 for value, bit 0 for new data bit
	 * acc[3] = Y_AXIS_MSB
	 * acc[4] = Z_AXIS_LSB -> bit 7~4 for value, bit 0 for new data bit
	 * acc[5] = Z_AXIS_MSB
	 */
	for (i = X; i <= Z; i++)
		v[i] = (((int8_t)acc[i * 2 + 1]) << 8) | (acc[i * 2] & 0xf0);
	rotate(v, *s->rot_standard_ref, v);

	return EC_SUCCESS;
}

static int init(struct motion_sensor_t *s)
{
	int ret = 0;
	int val, reg, reset_field;

	/* This driver requires a mutex */
	ASSERT(s->mutex);

	ret = raw_read8(s->port, s->i2c_spi_addr_flags,
			REG_CHIP_ID, &val);
	if (ret)
		return EC_ERROR_UNKNOWN;

	if (val != MIR3DA_CHIPID)
		return EC_ERROR_ACCESS_DENIED;

	/* Reset the chip to be in a good state */
	reg = MIR3DA_SPI_CONFIG;
	reset_field = MIR3DA_RESET_VALUE;

	mutex_lock(s->mutex);

	ret = raw_read8(s->port, s->i2c_spi_addr_flags, reg, &val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}
	val |= reset_field;
	ret = raw_write8(s->port, s->i2c_spi_addr_flags, reg, val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	/* The SRST will be cleared when reset is complete. */
	msleep(20);
	mutex_unlock(s->mutex);

	return sensor_init_done(s);
}

const struct accelgyro_drv da217_accel_drv = {
	.init = init,
	.read = read,
	.set_range = set_range,
	.get_resolution = get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = get_data_rate,
	.set_offset = set_offset,
	.get_offset = get_offset,
};
