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
	int v = (mpu6050_read(reg) << 8) + mpu6050_read(reg+1);
	return v == 0xeeee ? 0 : v;
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
	if (mpu6050_read(MPU6050_REG_WHO_AM_I) != 0x68) {
		ccprintf("[6050] mpu6050_init() WHO_AM_I\n");
		return EC_ERROR_UNKNOWN;
	}

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
	const int rep_degree = 1;
	const char *axis = "XYZ";
	enum {
		LID_CLOSING,
		LID_CLOSED,
		LID_OPENED,
	} lid_state;
	int lid_counter;

	msleep(500);
	if (mpu6050_init() != EC_SUCCESS) {
		ccprintf("[6050] mpu6050_init() failed\n");
		msleep(1000000);
	}

	if (gpio_get_level(GPIO_LID_SWITCHn)) {
		lid_state = LID_OPENED;
		ccprintf("[6050] init:LID_OPENED\n");
	} else {
		lid_state = LID_CLOSED;
		ccprintf("[6050] init:LID_CLOSED\n");
	}
	lid_counter = 30;

	while (1) {

		msleep(100);
		val[0] = mpu6050_xout();
		val[1] = mpu6050_yout();
		val[2] = mpu6050_zout();
		for (i = 0; i < 3; ++i) {
			if (my_abs(val[i]) <= threshold)
				val[i] = 0;
			acc[i] += val[i];
			if (my_abs(acc[i] - last_rep[i]) >= (rep_degree * divider)) {
				last_rep[i] = acc[i];
				ccprintf("[6050] %c: %d\n", axis[i], acc[i] / divider);
			}
		}

		switch (lid_state) {
		case LID_CLOSING:
			if (lid_counter-- <= 0) {
				lid_state = LID_CLOSED;
				acc[0] = acc[1] = acc[2] = 0;
				ccprintf("[6050] counter<=0 ==> LID_CLOSED\n");
			}
			break;
		case LID_CLOSED:
			if (gpio_get_level(GPIO_LID_SWITCHn)) {
				lid_state = LID_OPENED;
				ccprintf("[6050] ==> LID_OPENED\n");
			}
			break;
		case LID_OPENED:
			if (gpio_get_level(GPIO_LID_SWITCHn) == 0) {
				lid_state = LID_CLOSING;
				lid_counter = 30;
				ccprintf("[6050] ==> LID_CLOSING\n");
			}
			break;
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

