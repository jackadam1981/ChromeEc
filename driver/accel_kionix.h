/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Kionix Accelerometer driver for Chrome EC */

#ifndef __CROS_EC_ACCEL_KIONIX_H
#define __CROS_EC_ACCEL_KIONIX_H

#include "common.h"

enum kionix_accel {
	KX022,
	KXCJ9,
	SUPPORTED_KIONIX_ACCELS,
};

/*
 * Struct for pairing an engineering value with the register value for a
 * parameter.
 */
struct accel_param_pair {
	int val; /* Value in engineering units. */
	int reg; /* Corresponding register value. */
};

struct kionix_accel_data {
	/* Variant of Kionix Accelerometer. */
	uint8_t variant;
	/* Note, the following are indicies into their respective tables. */
	/* Current range of accelerometer. */
	int sensor_range;
	/* Current output data rate of accelerometer. */
	int sensor_datarate;
	/* Current resolution of accelerometer. */
	int sensor_resolution;
	/* Device address. */
	int accel_addr;
	int16_t offset[3];
};

extern const struct accelgyro_drv kionix_accel_drv;

#endif /* __CROS_EC_ACCEL_KIONIX_H */
