/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* KXCJ9 gsensor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "accel_kxcj9.h"
#include "gpio.h"
#include "i2c.h"
#include "util.h"

/**
 * Read register from accelerometer.
 */
static int raw_read8(const int addr, const int reg, int *data_ptr)
{
	return i2c_read8(I2C_PORT_ACCEL, addr, reg, data_ptr);
}

/**
 * Write register from accelerometer.
 */
static int raw_write8(const int addr, const int reg, int data)
{
	return i2c_write8(I2C_PORT_ACCEL, addr, reg, data);
}


int read_accel(int addr, int *x_acc, int *y_acc, int *z_acc)
{
	uint8_t acc[6];
	uint8_t reg = KXCJ9_XOUT_L;
	int ret;

	/* Read 6 bytes starting at KXCJ9_XOUT_L. */
	ret = i2c_xfer(I2C_PORT_ACCEL, addr, &reg, 1, acc, 6, I2C_XFER_SINGLE);

	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * Convert acceleration to a signed 12-bit number. Note, based on
	 * the order of the registers:
	 *
	 * acc[0] = KXCJ9_XOUT_L
	 * acc[1] = KXCJ9_XOUT_H
	 * acc[2] = KXCJ9_YOUT_L
	 * acc[3] = KXCJ9_YOUT_H
	 * acc[4] = KXCJ9_ZOUT_L
	 * acc[5] = KXCJ9_ZOUT_H
	 */
	*x_acc = (((int8_t)acc[1]) << 4) | (acc[0] >> 4);
	*y_acc = (((int8_t)acc[3]) << 4) | (acc[2] >> 4);
	*z_acc = (((int8_t)acc[5]) << 4) | (acc[4] >> 4);

	return EC_SUCCESS;
}

void accel_init(int addr)
{
	/* Enable accelerometer, 12-bit resolution mode, +/- 2G range.*/
	raw_write8(addr,  KXCJ9_CTRL1,
			KXCJ9_CTRL1_PC1 | KXCJ9_CTRL1_RES | KXCJ9_GSEL_2G);

	/* Set output data rate. */
	raw_write8(addr,  KXCJ9_DATA_CTRL, KXCJ9_OSA_50_00HZ);
}



/*****************************************************************************/
/* Console commands */

static int command_read_accelerometer(int argc, char **argv)
{
	char *e;
	int addr, reg, data;

	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is address. */
	addr = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	/* Second argument is register offset. */
	reg = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	raw_read8(addr, reg, &data);

	ccprintf("0x%02x\n", data);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accelread, command_read_accelerometer,
	"addr reg",
	"Read from accelerometer at slave address addr", NULL);

static int command_write_accelerometer(int argc, char **argv)
{
	char *e;
	int addr, reg, data;

	if (argc != 4)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is address. */
	addr = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	/* Second argument is register offset. */
	reg = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	/* Third argument is data. */
	data = strtoi(argv[3], &e, 0);
	if (*e)
		return EC_ERROR_PARAM3;

	raw_write8(addr, reg, data);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accelwrite, command_write_accelerometer,
	"addr reg data",
	"Write to accelerometer at slave address addr", NULL);
