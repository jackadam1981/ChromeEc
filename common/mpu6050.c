/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * MPU6050 driver.
 */

#include "board.h"
#include "console.h"
#include "gpio.h"
#include "i2c.h"
#include "mpu6050.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

#define MPU6050_I2C_ADDR 0xd0
#define MPU6050_PORT I2C_PORT_BATTERY

#define X_OFFSET 13
#define Y_OFFSET 53
#define Z_OFFSET 13

static int acc[3] = {0, 0, 0};

inline int my_abs(int x)
{
	return x < 0 ? -x : x;
}

inline int mpu6050_write(uint8_t reg, uint8_t val)
{
	return i2c_write8(MPU6050_PORT, MPU6050_I2C_ADDR, reg, val);
}

inline int mpu6050_read(uint8_t reg)
{
	int val = 0;
	if (i2c_read8(MPU6050_PORT, MPU6050_I2C_ADDR, reg, &val))
		return 0xee;
	return val;
}

inline int mpu6050_read16(uint8_t reg)
{
	return (mpu6050_read(reg) << 8) + mpu6050_read(reg+1);
}

inline int mpu6050_xout(void)
{
	return (int16_t)mpu6050_read16(MPU6050_REG_GYRO_XOUT_H) - X_OFFSET;
}

inline int mpu6050_yout(void)
{
	return (int16_t)mpu6050_read16(MPU6050_REG_GYRO_YOUT_H) - Y_OFFSET;
}

inline int mpu6050_zout(void)
{
	return (int16_t)mpu6050_read16(MPU6050_REG_GYRO_ZOUT_H) - Z_OFFSET;
}

inline int mpu6050_init(void)
{
	if (mpu6050_read(MPU6050_REG_WHO_AM_I) != 0x68)
		return EC_ERROR_UNKNOWN;

	/* Sample rate: 125Hz */
	/* 16.4LSB/degree/s */
	return (mpu6050_write(MPU6050_REG_PWR_MGMT_1, 0x00) ||
		mpu6050_write(MPU6050_REG_SMPLRT_DIV, 0x07) ||
		mpu6050_write(MPU6050_REG_CONFIG, 0x06) ||
		mpu6050_write(MPU6050_REG_GYRO_CONFIG, 0x18));
}

void mpu6050_task(void)
{
	int last_rep[3] = {0, 0, 0};
	int val[3];
	int i;
	const int divider = 164;
	const int threshold = 10;
	const int rep_degree = 5;
	const char *axis = "XYZ";

	msleep(500);
	mpu6050_init();

	while (1) {
		msleep(100);
		val[0] = mpu6050_xout();
		val[1] = mpu6050_yout();
		val[2] = mpu6050_zout();
		for (i = 0; i < 3; ++i) {
			if (my_abs(val[i]) <= threshold)
				val[i] = 0;
			acc[i] += val[i];
			if (my_abs(acc[i] - last_rep[i]) >= rep_degree * divider) {
				last_rep[i] = acc[i];
				ccprintf("[6050] %c: %d\n", axis[i], acc[i] / divider);
			}
		}
	}
}

/*****************************************************************************/
/* Console commands */

static int command_mpu6050(int argc, char **argv)
{
	if (argc == 1) {
		ccprintf("X = %d\n", mpu6050_xout());
		ccprintf("Y = %d\n", mpu6050_yout());
		ccprintf("Z = %d\n", mpu6050_zout());
	} else if (!strcasecmp(argv[1], "acc")) {
		ccprintf("ACC_X = %d\n", acc[0]);
		ccprintf("ACC_Y = %d\n", acc[1]);
		ccprintf("ACC_Z = %d\n", acc[2]);
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(mpu6050, command_mpu6050,
			NULL,
			NULL,
			NULL);

#if 0
static int command_lp5562(int argc, char **argv)
{
	if (argc == 4) {
		char *e;
		uint8_t red, green, blue;

		red = strtoi(argv[1], &e, 0);
		if (e && *e)
			return EC_ERROR_PARAM1;
		green = strtoi(argv[2], &e, 0);
		if (e && *e)
			return EC_ERROR_PARAM2;
		blue = strtoi(argv[3], &e, 0);
		if (e && *e)
			return EC_ERROR_PARAM3;

		return lp5562_set_color(red, green, blue);
	} else if (argc == 2) {
		if (!strcasecmp(argv[1], "on"))
			return lp5562_poweron();
		else if (!strcasecmp(argv[1], "off"))
			return lp5562_poweroff();
		return EC_ERROR_PARAM1;
	}

	return EC_ERROR_INVAL;
}
DECLARE_CONSOLE_COMMAND(lp5562, command_lp5562,
			"on | off | <red> <green> <blue>",
			"Set the color of the LED",
			NULL);
#endif
