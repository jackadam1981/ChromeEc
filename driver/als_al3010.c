/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Dyna-Image AL3010 light sensor driver
 */

#include "driver/als_al3010.h"
#include "i2c.h"

/**
 * AL3010 Check interrupt status
 */
int al3010_check_interrupt(int *val)
{
	int ret;

	ret = i2c_read8(I2C_PORT_ALS, AL3010_I2C_ADDR,
			AL3010_REG_INT_STATUS, val);

	if (ret)
		return ret;

	return EC_SUCCESS;
}

/**
 * Get AL3010 lux
 */
int al3010_get_lux(int *lux, int af)
{
	int ret;
	int val;
	long long val64;

	ret = i2c_read16(I2C_PORT_ALS, AL3010_I2C_ADDR,
			 AL3010_REG_DATA_LOW, &val);

	if (ret)
		return ret;

	val64 = val;
	val64 = (val64 * AL3010_GAIN_SCALE) / 10000;
	val = val64 * af / 100;

	*lux = val;

	return EC_SUCCESS;
}

/**
 * Initialise AL3010 light sensor.
 */
int al3010_init(void)
{
	int ret;

	ret = i2c_write8(I2C_PORT_ALS, AL3010_I2C_ADDR,
			 AL3010_REG_CONFIG, AL3010_GAIN << 4);
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

	ret = al3010_get_lux(lux, af);

	return ret;
}
