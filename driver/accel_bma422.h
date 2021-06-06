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
 * The addr field of motion_sensor support both SPI and I2C:
 * This is defined in include/i2c.h and is no longer an 8bit
 * address. The 7/10 bit address starts at bit 0 and leaves
 * room for a 10 bit address, although we don't currently
 * have any 10 bit slaves. I2C or SPI is indicated by a
 * more significant bit
 */

/* I2C addresses */
#define BMA422_ADDR0_FLAGS      0x18


struct bma422_accel_drv_data {
	struct accelgyro_saved_data_t saved_data;
	/* Current resolution of accelerometer. */
	int sensor_resolution;
	int16_t offset[3];
};

#endif /* __CROS_EC_ACCEL_BMA422_H */
