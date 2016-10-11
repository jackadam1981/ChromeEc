/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ALS_H
#define __CROS_EC_ALS_H

#include "common.h"

/* Priority for ALS HOOK int */
#define HOOK_PRIO_ALS_INIT (HOOK_PRIO_DEFAULT + 1)

/* Defined in board.h */
enum als_id;

/* ALS Driver */
struct als_driver {
	/* ALS name */
	const char *name;

	/**
	 * Init an ALS
	 *
	 * @return EC_SUCCESS, or non-zero if error.
	 */
	int (*init)(void);

	/**
	 * Read an ALS
	 *
	 * @param id		Which one?
	 * @param lux	        Put value here
	 *
	 * @return EC_SUCCESS, or non-zero if error.
	 */
	int (*read)(int *lux, const int af);
};

/* Initialized in board.c */
struct als_t {
	const int attenuation_factor;
	const struct als_driver *drv;
};

extern const struct als_t als[];

#endif  /* __CROS_EC_ALS_H */
