/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* BMI323 accelerometer and gyro for Chrome EC */

#ifndef __CROS_EC_DRIVER_ACCELGYRO_BMI323_PUBLIC_H
#define __CROS_EC_DRIVER_ACCELGYRO_BMI323_PUBLIC_H

/*
 * The addr field of motion_sensor support both SPI and I2C:
 * This is defined in include/i2c.h and is no longer an 8bit
 * address. The 7/10 bit address starts at bit 0 and leaves
 * room for a 10 bit address, although we don't currently
 * have any 10 bit slaves.  I2C or SPI is indicated by a
 * more significant bit
 */

struct bmi323_drv_data {
	struct accelgyro_saved_data_t saved_data[3];
	uint8_t flags;
	uint8_t enabled_activities;
	uint8_t disabled_activities;
	/* Current resolution of accelerometer. */
	int sensor_resolution;
	int16_t offset[3];
};

#define BMI3_GET_DATA(_s) \
	((struct bmi323_drv_data *)(_s)->drv_data)

#define BMI3_GET_SAVED_DATA(_s) \
	(&BMI3_GET_DATA(_s)->saved_data)

#define BMI3_DRDY_OFF(_sensor)   (7 - (_sensor))
#define BMI3_DRDY_MASK(_sensor)  (1 << BMI3_DRDY_OFF(_sensor))

extern const struct accelgyro_drv bmi3_drv;

void bmi323_interrupt(enum gpio_signal signal);

#ifdef CONFIG_CMD_I2C_STRESS_TEST_ACCEL
extern struct i2c_stress_test_dev bmi260_i2c_stress_test_dev;
#endif

#endif /* __CROS_EC_DRIVER_ACCELGYRO_BMI323_PUBLIC_H */
