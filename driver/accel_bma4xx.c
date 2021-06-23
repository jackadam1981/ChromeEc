/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Bosch Accelerometer driver for Chrome EC
 *
 * Supported: BMA422
 */

#include "accelgyro.h"
#include "accel_bma422.h"
#include "accel_bma4xx.h"
#include "common.h"
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

/*
 * Read register from accelerometer.
 */
static inline int raw_read8(const int port, const uint16_t i2c_addr_flags,
			    const int reg, int *data_ptr)
{
	return i2c_read8(port, i2c_addr_flags, reg, data_ptr);
}

/*
 * Write register from accelerometer.
 */
static inline int raw_write8(const int port, const uint16_t i2c_addr_flags,
			     const int reg, int data)
{
	int ret;

	ret = i2c_write8(port, i2c_addr_flags, reg, data);

	/* Wait for 450 usec for sensor to process */
	usleep(450);

	return ret;
}

static int set_range(struct motion_sensor_t *s, int range, int rnd)
{
	int ret,  range_val;

	range_val = BMA4_RANGE_TO_REG(range);

	/*
	 * If rounding flag is set then set the range_val to nearest
	 * valid value
	 */
	if ((BMA4_RANGE_TO_REG(range_val) < range) && rnd)
		range_val = BMA4_RANGE_TO_REG(range * 2);

	mutex_lock(s->mutex);

	/* Determine the new value of control reg and attempt to write it. */
	ret = i2c_field_update8(s->port, s->i2c_spi_addr_flags,
			BMA4_ACCEL_RANGE_ADDR, BMA4_ACCEL_RANGE_MSK, range_val);

	/* If successfully written, then save the range. */
	if (ret == EC_SUCCESS)
		s->current_range = BMA4_REG_TO_RANGE(range_val);

	mutex_unlock(s->mutex);

	return ret;
}

static int get_resolution(const struct motion_sensor_t *s)
{
	return BMA4_12_BIT_RESOLUTION;
}

static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret, odr_val, odr_reg_val, reg_val;
	struct accelgyro_saved_data_t *data = s->drv_data;

	odr_val = BMA4_ODR_TO_REG(rate);
	if ((BMA4_REG_TO_ODR(odr_val) < rate) && rnd)
		odr_val = BMA4_ODR_TO_REG(rate * 2);

	mutex_lock(s->mutex);

	/* Determine the new value of control reg and attempt to write it. */
	ret = raw_read8(s->port, s->i2c_spi_addr_flags,
			BMA4_ACCEL_CONFIG_ADDR, &odr_reg_val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}
	reg_val = (odr_reg_val & ~BMA4_ACCEL_ODR_MSK) | odr_val;

	/* Set output data rate. */
	ret = raw_write8(s->port, s->i2c_spi_addr_flags,
			 BMA4_ACCEL_CONFIG_ADDR, reg_val);

	/* If successfully written, then save the new data rate. */
	if (ret == EC_SUCCESS)
		data->odr = BMA4_REG_TO_ODR(odr_val);

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
	int i, ret;
	intv3_t v = { offset[X], offset[Y], offset[Z] };

	rotate_inv(v, *s->rot_standard_ref, v);

	/* temperature is ignored */
	/* Offset from host is in 1/1024g, 1/128g internally. */
	for (i = X; i <= Z; i++) {
		ret = raw_write8(s->port, s->i2c_spi_addr_flags,
				 BMA4_OFFSET_0_ADDR + i, v[i] / 8);
		RETURN_ERROR(ret);
	}
	return EC_SUCCESS;
}

static int get_offset(const struct motion_sensor_t *s, int16_t *offset,
		      int16_t *temp)
{
	int i, val, ret;
	intv3_t v;

	for (i = X; i <= Z; i++) {
		ret = raw_read8(s->port, s->i2c_spi_addr_flags,
				BMA4_OFFSET_0_ADDR + i, &val);
		if (ret)
			return ret;
		v[i] = (int8_t)val * 8;
	}
	rotate(v, *s->rot_standard_ref, v);
	offset[X] = v[X];
	offset[Y] = v[Y];
	offset[Z] = v[Z];

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
			     BMA4_DATA_8_ADDR, acc, 6);
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

static int perform_calib(struct motion_sensor_t *s, int enable)
{
	/* TODO */
	return EC_ERROR_UNIMPLEMENTED;
}

static int init(struct motion_sensor_t *s)
{
	int ret = 0, reg_val;

	/* This driver requires a mutex. Assert if mutex is not supplied. */
	ASSERT(s->mutex);

	/* Read accelerometer's CHID ID */
	ret = raw_read8(s->port, s->i2c_spi_addr_flags,
			BMA4_CHIP_ID_ADDR, &reg_val);
	if (ret)
		return EC_ERROR_UNKNOWN;

	if (s->chip != MOTIONSENSE_CHIP_BMA422 || reg_val != BMA422_CHIP_ID)
		return EC_ERROR_HW_INTERNAL;

	mutex_lock(s->mutex);

	/* Enable accelerometer */
	ret = i2c_field_update8(s->port, s->i2c_spi_addr_flags,
				BMA4_POWER_CTRL_ADDR, BMA4_POWER_ACC_EC_MASK,
				BMA4_POWER_ACC_EC_MASK);

	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	mutex_unlock(s->mutex);

	return sensor_init_done(s);
}

const struct accelgyro_drv bma4_accel_drv = {
	.init = init,
	.read = read,
	.set_range = set_range,
	.get_resolution = get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = get_data_rate,
	.set_offset = set_offset,
	.get_offset = get_offset,
	.perform_calib = perform_calib,
};
