/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "accel_cal.h"

#ifndef CONFIG_FPU
#error "CONFIG_FPU required for accelerometer calibration"
#endif

#define MSS_TO_G(v) (v * 0.101936799f)
#define G_TO_MSS(v) (v * 9.81f)

static void accel_cal_reset(struct accel_cal *cal)
{
	int i;

	for (i = 0; i < cal->num_temp_windows; ++i) {
		kasa_reset(&cal->algos[i].kasa_fit);
		newton_fit_reset(&cal->algos[i].newton_fit);
	}
}

bool accel_cal_accumulate(struct accel_cal *cal, uint32_t sample_time,
			  float x, float y, float z, float temp)
{
	struct accel_cal_algo *algo;

	x = MSS_TO_G(x);
	y = MSS_TO_G(y);
	z = MSS_TO_G(z);

	/* Test that we're within the temperature range. */
	if (temp >= CONFIG_ACCEL_CAL_MAX_TEMP ||
	    temp <= CONFIG_ACCEL_CAL_MIN_TEMP)
		return false;

	/* Test that we have a still sample. */
	if (!still_det_update(&cal->still_det, sample_time, x, y, z))
		return false;

	/* We have a still sample, update x, y, and z to the mean. */
	x = cal->still_det.mean_x;
	y = cal->still_det.mean_y;
	z = cal->still_det.mean_z;

	/* Compute the temp gate. */
	algo = &cal->algos[(int) ((temp - CONFIG_ACCEL_CAL_MIN_TEMP) /
				  (CONFIG_ACCEL_CAL_MAX_TEMP -
				   CONFIG_ACCEL_CAL_MIN_TEMP))];

	kasa_accumulate(&algo->kasa_fit, x, y, z);
	if (newton_fit_accumulate(&algo->newton_fit, x, y, z)) {
		float radius;

		kasa_compute(&algo->kasa_fit, cal->bias, &radius);
		if (ABS(radius - 1.0f) < CONFIG_ACCEL_CAL_KASA_RADIUS_THRES)
			goto accel_cal_accumulate_success;

		newton_fit_compute(&algo->newton_fit, cal->bias, &radius);
		if (ABS(radius - 1.0f) < CONFIG_ACCEL_CAL_NEWTON_RADIUS_THRES)
			goto accel_cal_accumulate_success;
	}

	return false;

accel_cal_accumulate_success:
	cal->bias[X] = G_TO_MSS(cal->bias[X]);
	cal->bias[Y] = G_TO_MSS(cal->bias[Y]);
	cal->bias[Z] = G_TO_MSS(cal->bias[Z]);

	return true;
}
