/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ALS_H
#define __CROS_EC_ALS_H

#include "common.h"
#include "motion_sense.h"

/* Priority for ALS HOOK int */
#define HOOK_PRIO_ALS_INIT (HOOK_PRIO_DEFAULT + 1)

/* Defined in board.h */
enum als_id;

/* ALS Driver */
struct als_driver {
	/* ALS name */
	const char *name;
	/* i2c port */
	uint8_t port;
	/* i2c address or SPI slave logic GPIO. */
	uint8_t addr;

	/**
	 * Init an ALS
	 *
	 * @param s Pointer to sensor data pointer.
	 *
	 * @return EC_SUCCESS, or non-zero if error.
	 */
	int (*init)(const struct motion_sensor_t *s);

	/**
	 * Read an ALS
	 *
	 * @param s Pointer to sensor data.
	 * @param v Vector to store acceleration (in units of counts).
	 *
	 * @return EC_SUCCESS, or non-zero if error.
	 */
	int (*read)(const struct motion_sensor_t *s, vector_3_t v);
};

/* Initialized in board.c */
struct als_t {
	const int attenuation_factor;
	const struct als_driver *drv;
};

extern const struct als_t als;

#endif  /* __CROS_EC_ALS_H */
