/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Dyna-Image AL3010 light sensor driver
 */

#include "driver/als_al3010.h"
#include "i2c.h"

static const int al3010_ranges[4] = {
	11872, 2968, 742, 186};
/**
 * Initialise AL3010 light sensor.
 */
int al3010_init(void)
{
	int ret;

	ret = i2c_write8(I2C_PORT_ALS, AL3010_I2C_ADDR,
			 AL3010_REG_CONFIG, AL3010_RANGE << 4);
	if (ret)
		return ret;

	return i2c_write8(I2C_PORT_ALS, AL3010_I2C_ADDR,
			  AL3010_REG_SYSTEM, AL3010_ENABLE);
}

/**
 * Read AL3010 light sensor data.
 */
int al3010_read_lux(int *lux, int af)
{
	int ret;
	int val;
	long int val64;

	ret = i2c_read16(I2C_PORT_ALS, AL3010_I2C_ADDR,
			 AL3010_REG_DATA_LOW, &val);
	if (ret)
		return ret;

	val64 = val;
	val64 = (val64 * al3010_ranges[AL3010_RANGE]) / 10000;
	val = val64 * af / 100;

	*lux = val;

	return EC_SUCCESS;
}
