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
#include "pwm.h"
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
#define X (acc[0] / divider)
static int moving_degree = 0;
static int previous_degree = 0;

static int servo_degree = 0;
static int servo_duty = 0;
static int knock_timer = 0;
static int last_knock = 0;

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

void my_pwm_enable(int enable) {
	pwm_enable_keyboard_backlight(enable);
}

void my_uuu(int u) {
	pwm_set_keyboard_backlight(100);
	udelay(u);
	pwm_set_keyboard_backlight(0);
}

#define MAX_DUTY 1000
int my_pwm_duty(int duty /* 0 ~ MAX_DUTY */) {
	int u;
	duty = MIN(MAX(duty, 0), MAX_DUTY);
	duty = MAX_DUTY - duty;
	u = duty * 1500 / MAX_DUTY + 500;
	my_uuu(u);
	return u;
}

int fade_step(int current_degree, int target_degree, int default_step) {
	int ret;
	int diff;
	int fade_degree;
	if (current_degree <= 10) {
		diff = MAX(current_degree, 5);
		fade_degree = 10;
	} else if (target_degree - current_degree < 20) {
		diff =  target_degree - current_degree;
		fade_degree = 20;
	} else {
		diff = 1;
		fade_degree = 1;
	}
	ret = default_step * diff / fade_degree;
	ccprintf("[6050] fade_step=%d\n", ret);
	return ret;
}

#define LOOP_MS 20
void mpu6050_task(void)
{
	int last_rep[3] = {0, 0, 0};
	int val[3];
	int i;
	const int divider = 164 * 100 / LOOP_MS;
	const int threshold = 10;
	const int rep_degree = 1;
	const char *axis = "XYZ";
	// LID
	enum {
		LID_CLOSING,
		LID_CLOSED,
		LID_OPENED,
	} lid_state;
	int lid_counter;
	// SERVO
	enum {
		SERVO_DISABLED,
		SERVO_FEEDING,
	} servo_state = SERVO_DISABLED;
	int servo_counter = 0;
	int servo_step = 0;

	msleep(500);
	if (mpu6050_init() != EC_SUCCESS) {
		ccprintf("[6050] mpu6050_init() failed\n");
		msleep(1000000);
	}

	gpio_enable_interrupt(GPIO_USB1_ILIM_SEL);

	if (gpio_get_level(GPIO_LID_SWITCHn)) {
		lid_state = LID_OPENED;
		ccprintf("[6050] init:LID_OPENED\n");
	} else {
		lid_state = LID_CLOSED;
		ccprintf("[6050] init:LID_CLOSED\n");
		acc[0] = acc[1] = acc[2] = 0;
	}
	lid_counter = 3000 / LOOP_MS;

	while (1) {
		int loop_us = LOOP_MS * 1000;

		val[0] = mpu6050_xout();
		// val[1] = mpu6050_yout();
		// val[2] = mpu6050_zout();
		for (i = 0; i < 1; ++i) {
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
				servo_degree = 0;
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
				lid_counter = 1000 / LOOP_MS;
				ccprintf("[6050] ==> LID_CLOSING\n");

				#define MOV_SCALE 300 /* fixed-point */
				previous_degree = moving_degree / MOV_SCALE;
				ccprintf("[6050] prev = %d\n", previous_degree);
				moving_degree = 0;
			}
			/* moving average */
			if (X * MOV_SCALE < moving_degree)
				moving_degree = X * MOV_SCALE;
			else
				moving_degree =
				  (moving_degree * (MOV_SCALE - 1) / MOV_SCALE +
				  X * 1);
			break;
		}

		switch (servo_state) {
		case SERVO_DISABLED:
			if ((servo_degree && lid_state == LID_CLOSED) ||
			    servo_duty) {
				servo_state = SERVO_FEEDING;
				servo_counter = 6000 / LOOP_MS;
				servo_step = 0;
				loop_us -= my_pwm_duty(servo_step);
				my_pwm_enable(1);
				ccprintf("[6050] ==> SERVO_FEEDING\n");
			}
			break;
		case SERVO_FEEDING:
			if ((acc[0] <= (servo_degree * divider) &&
			     servo_duty == 0) ||
			    --servo_counter <= 0) {
				servo_state = SERVO_DISABLED;
				my_pwm_enable(0);
				loop_us -= my_pwm_duty(0);
				servo_degree = 0;
				servo_duty = 0;
				ccprintf("[6050] ==> SERVO_DISABLED\n");
			} else {
				#define STEP_SCALE 100
				if (servo_duty) {
					servo_step = -servo_duty * STEP_SCALE;
				} else {
					servo_step += fade_step(
					    -X, -servo_degree,
					    MAX_DUTY * STEP_SCALE /
					    (3000/*ms*/ / LOOP_MS));
				}
				loop_us -= my_pwm_duty(servo_step / STEP_SCALE);
				/* ccprintf("[6050] ==> SERVO_STEP: %d\n",
				            servo_step); */
			}
			break;
		}

		usleep(loop_us);
		knock_timer++;
	}
}

void mic_interrupt(enum gpio_signal signal) {
	int interval = knock_timer - last_knock;

	ccprintf("interval=%d\n", interval);
	if (interval >= (100/*ms*/ / LOOP_MS) &&
	    interval <= (350/*ms*/ / LOOP_MS)) {
		servo_degree = previous_degree;
	}

	last_knock = knock_timer;
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
		ccprintf("servo_degree = %d\n", servo_degree);
		ccprintf("moving_degree = %d\n", moving_degree);
		ccprintf("previous_degree = %d\n", previous_degree);
	} else if (!strcasecmp(argv[1], "prev")) {
		previous_degree = -atoi(argv[2]);
		ccprintf("Set previous_degree=%d\n", previous_degree);
	} else if (!strcasecmp(argv[1], "deg")) {
		servo_degree = -atoi(argv[2]);
		ccprintf("Set servo_degree=%d\n", servo_degree);
	} else if (!strcasecmp(argv[1], "duty")) {
		servo_duty = -atoi(argv[2]);
		ccprintf("Set servo_duty=%d\n", servo_duty);
	} else if (!strcasecmp(argv[1], "zero")) {
		acc[0] = acc[1] = acc[2] = 0;
	} else if (!strcasecmp(argv[1], "ping")) {
		int val = mpu6050_read(MPU6050_REG_WHO_AM_I);
		ccprintf("Ping %s\n", val == 0x68 ? "OK" : "FAILED");
	} else if (!strcasecmp(argv[1], "init")) {
		if (mpu6050_init() != EC_SUCCESS) {
			ccprintf("[6050] mpu6050_init() failed\n");
		} else {
			ccprintf("[6050] mpu6050_init() SUCCESS\n");
		}
	} else if (!strcasecmp(argv[1], "uuu")) {
		my_uuu(atoi(argv[2]));
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(mpu6050, command_mpu6050,
			NULL,
			NULL,
			NULL);

