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
#include "common.h"
#include "console.h"
#include "accel_bma422.h"
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
	return i2c_write8(port, i2c_addr_flags, reg, data);
}

static int set_range(struct motion_sensor_t *s, int range, int rnd)
{
	return 0;
}

static int get_resolution(const struct motion_sensor_t *s)
{
	return 0;
}

static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	return 0;
}

static int get_data_rate(const struct motion_sensor_t *s)
{
	return 0;
}

static int set_offset(const struct motion_sensor_t *s, const int16_t *offset,
		      int16_t temp)
{
	return EC_SUCCESS;
}

static int get_offset(const struct motion_sensor_t *s, int16_t *offset,
		      int16_t *temp)
{
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
	return 0;
}

static int init(struct motion_sensor_t *s)
{
	int ret = 0, val;

	/* This driver requires a mutex. Assert if mutex is not supplied. */
	ASSERT(s->mutex);

	/* Read accelerometer's CHID ID */
	ret = raw_read8(s->port, s->i2c_spi_addr_flags,
			BMA4_CHIP_ID_ADDR, &val);
	if (ret)
		return EC_ERROR_UNKNOWN;

	if (val != BMA422_CHIP_ID)
		return EC_ERROR_ACCESS_DENIED;

	mutex_lock(s->mutex);

	/* Enable accelerometer */
	ret = raw_read8(s->port, s->i2c_spi_addr_flags,
			BMA4_POWER_CTRL_ADDR, &val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	val |= BMA4_POWER_ACC_EC_MASK;

	ret = raw_write8(s->port, s->i2c_spi_addr_flags,
			BMA4_POWER_CTRL_ADDR, val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	mutex_unlock(s->mutex);

	return sensor_init_done(s);
}

const struct accelgyro_drv bma422_accel_drv = {
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
