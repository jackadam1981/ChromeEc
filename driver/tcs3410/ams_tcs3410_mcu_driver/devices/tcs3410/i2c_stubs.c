/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

/*
 * This file contains the i2c stub routines that are platform and architecure
 * specific and are essential to proper operation of the ams device driver.
 *
 * These are just examples of possible candidates for the i2c interface.  The
 * i2c APIs are called from mainly the sensor driver.  Here are some examples
 * where the i2c APIs are used:
 *    - sensor_read()
 *    - sensor_write()
 *    - sensor_modify()
 */

#include <stdint.h>
#include <stdio.h>

#include "common.h"
#include "i2c.h"
#include "ams_i2c.h"
#include "ams_device.h"

int ams_i2c_block_read(uint8_t addr, uint8_t reg, uint8_t *data, int size)
{
	return i2c_read_block(CONFIG_ALS_TCS3410_PORT, addr, reg, data, size);
}

int ams_i2c_read(uint8_t addr, uint8_t reg, int *data)
{
	return i2c_read8(CONFIG_ALS_TCS3410_PORT, addr, reg, data);
}

int ams_i2c_block_write(uint8_t addr, uint8_t reg, uint8_t *data, int size)
{
	return i2c_write_block(CONFIG_ALS_TCS3410_PORT, addr, reg, data, size);
}

int ams_i2c_write(uint8_t addr, uint8_t *sh, uint8_t reg, int data)
{
	int ret = ams_i2c_write_direct(addr, reg, data);

	if (ret == EC_SUCCESS)
		sh[reg] = data;

	return ret;
}

int ams_i2c_write_direct(uint8_t addr, uint8_t reg, uint8_t data)
{
	return i2c_write8(CONFIG_ALS_TCS3410_PORT, addr, reg, data);
}

int ams_i2c_modify(uint8_t addr, uint8_t *sh, uint8_t reg, uint8_t mask, uint8_t val)
{
	int temp;

	ams_i2c_read(addr, reg, &temp);
	temp &= ~mask;
	temp |= val;
	return ams_i2c_write(addr, sh, reg, temp);
}
