/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LIS2DW12 gsensor module for Chrome EC */

#ifndef __CROS_EC_DRIVER_ACCEL_LIS2DW12_PUBLIC_H
#define __CROS_EC_DRIVER_ACCEL_LIS2DW12_PUBLIC_H

extern const struct accelgyro_drv lis2dw12_accel_drv;

/* I2C ADDRESS DEFINITIONS    */
#define LIS2DW12_ADDR0			0x18
#define LIS2DW12_ADDR1			0x19

#define LIS2DWL_ADDR0_FLAGS		0x18
#define LIS2DWL_ADDR1_FLAGS		0x19

/* Min and Max sampling frequency in mHz. */
#define LIS2DW12_ODR_MIN_VAL		12500
#define LIS2DW12_ODR_MAX_VAL		\
	MOTION_MAX_SENSOR_FREQUENCY(1600000, LIS2DW12_ODR_MIN_VAL)

#endif /* __CROS_EC_DRIVER_ACCEL_LIS2DW12_PUBLIC_H */
