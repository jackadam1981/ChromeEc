/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* BMA422 gsensor module for Chrome EC */

#ifndef __CROS_EC_ACCEL_BMA422_H
#define __CROS_EC_ACCEL_BMA422_H

#include "accel_bma422.h"

extern const struct accelgyro_drv bma422_accel_drv;

/*
 * 7-bit address is 001111Xb. Where 'X' is determined
 * by the voltage on the ADDR pin.
 */
/* I2C addresses */
#define BMA422_ADDR0_FLAGS      0x18


struct bma422_accel_drv_data {
	struct accelgyro_saved_data_t saved_data;
	/* Current resolution of accelerometer. */
	int sensor_resolution;
	int16_t offset[3];
};

/* Min and Max sampling frequency in mHz */
#define BMA422_ACCEL_MIN_FREQ    12500
#define BMA422_ACCEL_MAX_FREQ    MOTION_MAX_SENSOR_FREQUENCY(1600000, 6250)

#endif /* __CROS_EC_ACCEL_BMA422_H */
