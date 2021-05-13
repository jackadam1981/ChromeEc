/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Modifications Copyright (C) 2021 Bosch Sensortec GmbH
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* BMA422 accelerometer for Chrome EC */

#ifndef __CROS_EC_DRIVER_ACCELGYRO_BMA422_PUBLIC_H
#define __CROS_EC_DRIVER_ACCELGYRO_BMA422_PUBLIC_H

/*
 * The addr field of motion_sensor support both SPI and I2C:
 * This is defined in include/i2c.h and is no longer an 8bit
 * address. The 7/10 bit address starts at bit 0 and leaves
 * room for a 10 bit address, although we don't currently
 * have any 10 bit slaves.  I2C or SPI is indicated by a
 * more significant bit
 */

/* I2C addresses */
//#define BMI3_ADDR0_FLAGS	0x68

extern const struct accelgyro_drv bma422_drv;


struct bma422_accel_drv_data {
	struct accelgyro_saved_data_t saved_data;
	/* Current resolution of accelerometer. */
	int sensor_resolution;

	int16_t offset[3];

//#ifdef CONFIG_KX022_ORIENTATION_SENSOR
//	int8_t raw_orientation;
//	enum motionsensor_orientation orientation;
//	enum motionsensor_orientation last_orientation;
//#endif
};
//void bmi260_interrupt(enum gpio_signal signal);

#ifdef CONFIG_CMD_I2C_STRESS_TEST_ACCEL
extern struct i2c_stress_test_dev bmi260_i2c_stress_test_dev;
#endif

#endif /* __CROS_EC_DRIVER_ACCELGYRO_BMI323_PUBLIC_H */
