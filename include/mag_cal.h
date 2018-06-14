/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Online magnetometer calibration */

#ifndef __CROS_EC_MAG_CAL_H
#define __CROS_EC_MAG_CAL_H

#include "math_util.h"
#include "mat44.h"
#include "timer.h"
#include "vec4.h"

#define MAG_CAL_MAX_SAMPLES 0xffff
#define MAG_CAL_MIN_BATCH_WINDOW_US    SECOND
#define MAG_CAL_MIN_BATCH_SIZE      25      /* samples */

struct mag_cal_t {
	/*
	 * Matric for sphere fitting:
	 * +----+----+----+----+
	 * | xx | xy | xz | x  |
	 * +----+----+----+----+
	 * | xy | yy | yz | y  |
	 * +----+----+----+----+
	 * | xz | yz | zz | z  |
	 * +----+----+----+----+
	 * | x  | y  | z  | 1  |
	 * +----+----+----+----+
	 */
	mat44_float_t acc;
	floatv4_t acc_w;
	float radius;

	/* Bound valid eigen value in sensor units */
	float min_eigen, max_eigen;

	intv3_t bias;

	/* number of samples needed to calibrate */
	uint16_t batch_size;
	uint16_t nsamples;
};

void mag_cal_setup(struct mag_cal_t *moc, uint16_t range, int odr);

int mag_cal_update(struct mag_cal_t *moc, intv3_t v);

int mag_cal_set_offset(struct mag_cal_t *cal, const intv3_t offset);
int mag_cal_get_offset(struct mag_cal_t *cal, intv3_t offset);

#endif  /* __CROS_EC_MAG_CAL_H */
