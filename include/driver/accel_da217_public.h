/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* DA217 accel module for Chrome EC */

#ifndef __CROS_EC_DRIVER_ACCEL_DA217_PUBLIC_H
#define __CROS_EC_DRIVER_ACCEL_DA217_PUBLIC_H

extern const struct accelgyro_drv da217_accel_drv;

/* I2C ADDRESS DEFINITIONS
 *
 * 7-bit address is 011000Xb. Where 'X' is determined
 * by the voltage on the ADDR pin.
 */
#define DA217_ADDR1_FLAGS             0x26
#define DA217_ADDR2_FLAGS             0x27

/* Absolute Acc rate. */
#define DA217_ACCEL_MIN_FREQ		12500
#define DA217_ACCEL_MAX_FREQ		\
	MOTION_MAX_SENSOR_FREQUENCY(1600000, DA217_ACCEL_MIN_FREQ)

#endif /* __CROS_EC_DRIVER_ACCEL_DA217_PUBLIC_H */
