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
#include "hooks.h"
#include "timer.h"
#include "task.h"
#include "util.h"

/* Define I2C address of the two accelerometers. */
#define ACCEL_ADDR_LID   KXCJ9_ADDR0
#define ACCEL_ADDR_BASE  KXCJ9_ADDR1

enum accel_axis {
	AXIS_X,
	AXIS_Y,
	AXIS_Z
};

static int accel_disp;
static int accel_interval_ms = 1000;

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

/**
 * Read acceleration of an accelerometer on given axis.
 *
 * @param addr Slave address of accelerometer.
 * @param axis Axis desired.
 */
static int read_accel(int addr, enum accel_axis axis)
{
	int lo, hi;
	int reg = 0;

	/* Set register to read based on axis requested. */
	switch (axis) {
	case AXIS_X:
		reg = KXCJ9_XOUT_L;
		break;
	case AXIS_Y:
		reg = KXCJ9_YOUT_L;
		break;
	case AXIS_Z:
		reg = KXCJ9_ZOUT_L;
		break;
	}

	/* Read two bytes, representing low and high bytes of accel data. */
	raw_read8(addr, reg, &lo);
	raw_read8(addr, reg + 1, &hi);

	/* Convert to a signed 12 bit value. */
	return (((int8_t)hi) << 4) | (lo>>4);
}

static void accel_init(void)
{
	/* Enable both accelerometers, 12-bit resolution mode, +/- 2G range.*/
	raw_write8(ACCEL_ADDR_LID,  KXCJ9_CTRL1,
			KXCJ9_CTRL1_PC1 | KXCJ9_CTRL1_RES | KXCJ9_GSEL_2G);
	raw_write8(ACCEL_ADDR_BASE, KXCJ9_CTRL1,
			KXCJ9_CTRL1_PC1 | KXCJ9_CTRL1_RES | KXCJ9_GSEL_2G);

	/* Set output data rate. */
	raw_write8(ACCEL_ADDR_LID,  KXCJ9_DATA_CTRL, KXCJ9_OSA_50_00HZ);
	raw_write8(ACCEL_ADDR_BASE, KXCJ9_DATA_CTRL, KXCJ9_OSA_50_00HZ);
}

void accel_task(void)
{
	int acc_x_lid, acc_y_lid, acc_z_lid;
	int acc_x_base, acc_y_base, acc_z_base;
	timestamp_t ts0, ts1;
	int wait_us;

	while (1) {
		ts0 = get_time();

		/* Read all accelerations. */
		acc_x_lid = read_accel(ACCEL_ADDR_LID, AXIS_X);
		acc_y_lid = read_accel(ACCEL_ADDR_LID, AXIS_Y);
		acc_z_lid = read_accel(ACCEL_ADDR_LID, AXIS_Z);

		acc_x_base = read_accel(ACCEL_ADDR_BASE, AXIS_X);
		acc_y_base = read_accel(ACCEL_ADDR_BASE, AXIS_Y);
		acc_z_base = read_accel(ACCEL_ADDR_BASE, AXIS_Z);

		/*
		 * TODO: Remove this if statement once we get proto boards:
		 * This is a workaround for the problem on pre-proto that the
		 * accelerometers are only powered when AP is on. Therefore, we
		 * can't just initialize the accelerometers once at the
		 * beginning of this task, we have to re-initialize every time
		 * that we detect that the accelerometers have been power
		 * cycled. The accelerometers return -1 if they are powered but
		 * have not been initialized and 0 if they are not powered.
		 */
		if ((acc_x_lid == -1 && acc_y_lid == -1 && acc_z_lid == -1) ||
		    (acc_x_lid == 0  && acc_y_lid == 0  && acc_z_lid == 0)) {
			accel_init();
		} else if (accel_disp) {
			ts1 = get_time();
			ccprintf("%d,\t", acc_x_lid);
			ccprintf("%d,\t", acc_y_lid);
			ccprintf("%d,\t", acc_z_lid);
			ccprintf("%d,\t", acc_x_base);
			ccprintf("%d,\t", acc_y_base);
			ccprintf("%d,\t", acc_z_base);
			ccprintf("%d\n", ts1.val-ts0.val);
		}

		ts1 = get_time();
		wait_us = accel_interval_ms*1000 - (ts1.val-ts0.val);
		if (wait_us > 0)
			task_wait_event(wait_us);
	}
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

static int command_ctrl_accels(int argc, char **argv)
{
	char *e;
	int val;

	if (argc > 3)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is on/off whether to display accel data. */
	if (argc > 1) {
		if (!parse_bool(argv[1], &val))
			return EC_ERROR_PARAM1;

		accel_disp = val;

		if (accel_disp)
			ccprintf("\nLidX\tLidY\tLidZ\tBaseX\tBaseY\tBaseZ\n");
	}

	/* Second arg changes the accel task time interval. */
	if (argc > 2) {
		val = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		accel_interval_ms = val;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accel, command_ctrl_accels,
	"on/off [interval]",
	"Control acceleration task.", NULL);
