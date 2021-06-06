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
#include "accel_bma2x2.h"
#include "i2c.h"
#include "math_util.h"
#include "spi.h"
#include "task.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

/* Number of times to attempt to enable sensor before giving up. */
#define SENSOR_ENABLE_ATTEMPTS 5

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
	return EC_SUCCESS;
}

static int perform_calib(struct motion_sensor_t *s, int enable)
{
	return 0;
}

static int init(struct motion_sensor_t *s)
{
	return 0;
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
