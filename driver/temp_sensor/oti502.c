/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* OTI502 temperature sensor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "oti502.h"
#include "i2c.h"
#include "hooks.h"
#include "util.h"
#include "timer.h"

static int temp_val_ambient;	/* Ambient is chip temperature*/
static int temp_val_object;		/* Object is IR temperature */
static int parameter_need_update = 0;
static int parameter_done = 1;
static uint8_t msb, lsb;

static int oti502_read_block(const int offset, uint8_t *data, int len)
{
	return i2c_read_block(I2C_PORT_THERMAL, OTI502_I2C_ADDR_FLAGS,
			 offset, data, len);
}

int oti502_get_val(int idx, int *temp_ptr)
{
	switch (idx) {
	case OTI502_IDX_AMBIENT:
		*temp_ptr = temp_val_ambient;
		break;
	case OTI502_IDX_OBJECT:
		*temp_ptr = temp_val_object;
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

static void temp_sensor_poll(void)
{
	uint8_t temp_val[6];

	if (parameter_need_update)
		return;

	memset(temp_val, 0, sizeof(temp_val));

	oti502_read_block(0x80, temp_val, sizeof(temp_val));

	if (temp_val[2] >= 0x80) {
		/* Treat temperature as 0 degree C if temperature is negative*/
		temp_val_ambient = 0;
		ccprintf("Temperature ambient is negative !\n");
	} else {
		temp_val_ambient = ((temp_val[1] << 8) + temp_val[0]) / 200;
		temp_val_ambient = C_TO_K(temp_val_ambient);
	}

	if (temp_val[5] >= 0x80) {
		/* Treat temperature as 0 degree C if temperature is negative*/
		temp_val_object = 0;
		ccprintf("Temperature object is negative !\n");
	} else {
		temp_val_object = ((temp_val[4] << 8) + temp_val[5]) / 200;
		temp_val_object = C_TO_K(temp_val_object);
	}
}
DECLARE_HOOK(HOOK_SECOND, temp_sensor_poll, HOOK_PRIO_TEMP_SENSOR);

static void read_oti502_params(void)
{
	int val = 0;

	i2c_write8(I2C_PORT_THERMAL, OTI502_I2C_ADDR_FLAGS, 0x0E, 0x30);
	msleep(1);
	i2c_read16(I2C_PORT_THERMAL, OTI502_I2C_ADDR_FLAGS, 0x86, &val);

	msb = val & 0xff;
	lsb = val >> 8;

	ccprintf("K-parameter: MSB:0x%x, LSB:0x%x %x!\n", msb, lsb, val);
}

static void parameter_update(void)
{
	uint8_t data;
	int rv;
	int val;

	if (parameter_done) {
		rv = i2c_write8(I2C_PORT_THERMAL, OTI502_I2C_ADDR_FLAGS, 0x0E, 0x74);
		msleep(1);
		rv = i2c_read16(I2C_PORT_THERMAL, OTI502_I2C_ADDR_FLAGS, 0x86, &val);

		if (rv)
			return;

		ccprintf("OTI502 FW VER: %x!\n", val);
		parameter_done = 0;
	} else
		return;

	read_oti502_params();

	if (msb != OTI502_K_MSB || lsb != OTI502_K_LSB) {
		parameter_need_update = 1;
		i2c_write8(I2C_PORT_THERMAL, OTI502_I2C_ADDR_FLAGS, 0x0E, 0x31);
		msleep(1);
		i2c_write8(I2C_PORT_THERMAL, OTI502_I2C_ADDR_FLAGS, 0x0F, OTI502_K_LSB);
		msleep(5);
		i2c_write8(I2C_PORT_THERMAL, OTI502_I2C_ADDR_FLAGS, 0x0E, 0x30);
		msleep(1);
		i2c_write8(I2C_PORT_THERMAL, OTI502_I2C_ADDR_FLAGS, 0x0F, OTI502_K_MSB);
		msleep(5);

		data = 0x06;
		i2c_xfer(I2C_PORT_THERMAL, 0x00, &data, 1, NULL, 0);
		msleep(600);
		parameter_need_update = 0;
	}
}
DECLARE_HOOK(HOOK_SECOND, parameter_update, HOOK_PRIO_TEMP_SENSOR_DONE);
