/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Commons acc/gyro function for ST sensors oin Chrome EC
 */

#include "accelgyro.h"
#include "common.h"
#include "i2c.h"
#include "st_commons.h"

/**
 * Read single register
 */
inline int raw_read8(const int port, const int addr, const int reg,
		     int *data_ptr)
{
	/* TODO: Implement SPI interface support */
	return i2c_read8(port, addr, reg, data_ptr);
}

/**
 * Write single register
 */
inline int raw_write8(const int port, const int addr, const int reg,
		      int data)
{
	/* TODO: Implement SPI interface support */
	return i2c_write8(port, addr, reg, data);
}

/**
 * Read n bytes for read
 * NOTE: Some chip use MSB for auto-increments in SUB address
 * MSB must be set for autoincrement in multi read when auto_inc
 * is set
 */
int raw_read_n(const int port, const int addr, const uint8_t reg,
	       uint8_t *data_ptr, const int len, int auto_inc)
{
	int rv = -EC_ERROR_PARAM1;
	uint8_t reg_a;

	if (len > 1 && auto_inc)
		reg_a = reg | I2C_AUTO_INC;

	/* TODO: Implement SPI interface support */
	i2c_lock(port, 1);
	rv = i2c_xfer(port, addr, &reg_a, 1, data_ptr, len, I2C_XFER_SINGLE);
	i2c_lock(port, 0);

	return rv;
}

 /**
 * write_data_with_mask - Write register with mask
 * @s: Motion sensor pointer
 * @reg: Device register
 * @mask: The mask to search
 * @data: Data pointer
 */
int write_data_with_mask(const struct motion_sensor_t *s, int reg,
			 uint8_t mask, uint8_t data)
{
	int err;
	int new_data = 0x00, old_data = 0x00;

	err = raw_read8(s->port, s->addr, reg, &old_data);
	if (err != EC_SUCCESS)
		return err;

	new_data = ((old_data & (~mask)) | ((data << __builtin_ctz(mask)) & mask));

	if (new_data == old_data)
		return EC_SUCCESS;

	return raw_write8(s->port, s->addr, reg, new_data);
}

 /**
 * set_resolution - Set bit resolution
 * @s: Motion sensor pointer
 * @res: Bit resolution
 * @rnd: Round bit
 *
 * TODO: must support multiple resolution
 */
int set_resolution(const struct motion_sensor_t *s, int res, int rnd)
{
	return EC_SUCCESS;
}

/**
 * set_offset - Set data offset
 * @s: Motion sensor pointer
 * @offset: offset vector
 * @temp: Temp.
 */
int set_offset(const struct motion_sensor_t *s,
	       const int16_t *offset, int16_t temp)
{
	struct stprivate_data *data = s->drv_data;

	data->offset[X] = offset[X];
	data->offset[Y] = offset[Y];
	data->offset[Z] = offset[Z];
	return EC_SUCCESS;
}

/**
 * get_offset - Get data offset
 * @s: Motion sensor pointer
 * @offset: offset vector
 * @temp: Temp.
 */
int get_offset(const struct motion_sensor_t *s,
	       int16_t *offset, int16_t *temp)
{
	struct stprivate_data *data = s->drv_data;

	offset[X] = data->offset[X];
	offset[Y] = data->offset[Y];
	offset[Z] = data->offset[Z];
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

/**
 * get_data_rate - Get data rate (ODR)
 * @s: Motion sensor pointer
 */
int get_data_rate(const struct motion_sensor_t *s)
{
	struct stprivate_data *data = s->drv_data;

	return data->base.odr;
}

/**
 * normalize - Apply to LSB data sensitivity and rotation
 * @s: Motion sensor pointer
 * @data: LSB raw data
 */
void normalize(const struct motion_sensor_t *s, int *axis, uint8_t *data)
{
	int i;
	struct stprivate_data *drvdata = s->drv_data;

	for (i = X; i <= Z; i++) {
		/* Adjust data to sensor Sensitivity and Precision */
		if (drvdata->resol == 10)
			axis[i] = ((int16_t)((data[i * 2 + 1] << 8) | data[i * 2]) >> 6);
		else if (drvdata->resol == 16)
			axis[i] = ((int16_t)((data[i * 2 + 1] << 8) | data[i * 2]));

		/* Multiply axis gain related to FS */
		axis[i] = axis[i] * drvdata->base.range;
	}

	rotate(axis, *s->rot_standard_ref, axis);
}



