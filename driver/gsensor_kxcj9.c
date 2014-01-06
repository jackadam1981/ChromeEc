/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* KXCJ9 gsensor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "gsensor_kxcj9.h"
#include "gpio.h"
#include "i2c.h"
#include "hooks.h"
#include "util.h"

enum kxcj9_resolution {
	RES_8BIT = 0,
	RES_12BIT,
	NUM_KXCJ9_RESOLUTION,
};

static enum kxcj9_resolution resolution = RES_8BIT;

/**
 * Determine whether the sensor is powered.
 *
 * @return non-zero if the sensor is powered.
 */
static int has_power(void)
{
#ifdef CONFIG_GSENSOR_POWER_GPIO
	return gpio_get_level(CONFIG_GSENSOR_POWER_GPIO);
#else
	return 1;
#endif
}

static int raw_read8(const int addr, const int offset, int *data_ptr)
{
	return i2c_read8(I2C_PORT_GSENSOR, addr, offset, data_ptr);
}

/* 
static int raw_write8(const int addr, const int offset, int data)
{
	return i2c_write8(I2C_PORT_GSENSOR, addr, offset, data);
}
*/ 

static int get_axis(const int axis_offset, int *value_ptr)
{
	int rv;
	int raw = 0;

	rv = raw_read8(KXCJ9_ADDR0, axis_offset, &raw);
	if (rv < 0)
		return rv;

	if (resolution == RES_8BIT) {
		*value_ptr = (int)(int8_t)raw;
		return EC_SUCCESS;
	}
	return EC_ERROR_UNIMPLEMENTED;
}

static int print_status(void)
{
	int axis_value;

	axis_value = get_axis(KXCJ9_XOUT_L, &axis_value);
	return EC_SUCCESS;
}

static int command_gsensor(int argc, char **argv)
{
	if (!has_power()) {
		ccprintf("ERROR: Sensor not powered.\n");
		return EC_ERROR_NOT_POWERED;
	}

	return print_status();
}
DECLARE_CONSOLE_COMMAND(gsensor, command_gsensor,
	"Raw accelerometer data.",
	"Print kxcj9 gsensor value and status.", NULL);
