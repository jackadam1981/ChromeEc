/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include "stillness_detector.h"
#include "timer.h"

static void still_det_reset(struct still_det *still_det)
{
	still_det->num_samples = 0;
	still_det->acc_x = 0.0f;
	still_det->acc_y = 0.0f;
	still_det->acc_z = 0.0f;
	still_det->acc_xx = 0.0f;
	still_det->acc_yy = 0.0f;
	still_det->acc_zz = 0.0f;
}

static int stillness_batch_complete(struct still_det *still_det,
				    uint32_t sample_time)
{
	int complete = 0;
	uint32_t batch_window = time_until(still_det->window_start_time,
					   sample_time);

	/* Checking if enough data is accumulated */
	if (batch_window >= still_det->min_batch_window &&
	    still_det->num_samples > still_det->min_batch_size) {
		if (batch_window <= still_det->max_batch_window) {
			complete = 1;
		} else {
			/* Checking for too long batch window, reset and start
			 * over
			 */
			still_det_reset(still_det);
		}
	} else if (batch_window > still_det->min_batch_window &&
		   still_det->num_samples < still_det->min_batch_size) {
		/* Not enough samples collected, reset and start over */
		still_det_reset(still_det);
	}
	return complete;
}

void still_det_init(struct still_det *still_det, float var_threshold,
		    uint32_t min_batch_window, uint32_t max_batch_window,
		    uint32_t min_batch_size)
{
	memset(still_det, 0, sizeof(struct still_det));
	still_det->var_threshold = var_threshold;
	still_det->min_batch_window = min_batch_window;
	still_det->max_batch_window = max_batch_window;
	still_det->min_batch_size = min_batch_size;
}

int still_det_update(struct still_det *still_det, uint32_t sample_time,
		     float x, float y, float z)
{
	float inv = 0.0f, var_x, var_y, var_z;
	int complete = 0;

	/* Accumulate for mean and VAR */
	still_det->acc_x += x;
	still_det->acc_y += y;
	still_det->acc_z += z;
	still_det->acc_xx += x * x;
	still_det->acc_yy += y * y;
	still_det->acc_zz += z * z;

	/* Set a new start time if new batch */
	if (++still_det->num_samples == 1)
		still_det->window_start_time = sample_time;

	if (stillness_batch_complete(still_det, sample_time)) {
		/* Compute 1/num_samples and check for num_samples == 0 (should
		 * never happen, but just in case)
		 */
		if (still_det->num_samples) {
			inv = 1.0f / still_det->num_samples;
		} else {
			still_det_reset(still_det);
			return complete;
		}
		/* Calculating the VAR = sum(x^2)/n - sum(x)^2/n^2 */
		var_x = (still_det->acc_xx -
			 (still_det->acc_x * still_det->acc_x) * inv) * inv;
		var_y = (still_det->acc_yy -
			 (still_det->acc_y * still_det->acc_y) * inv) * inv;
		var_z = (still_det->acc_zz -
			 (still_det->acc_z * still_det->acc_z) * inv) * inv;
		/* Checking if sensor is still */
		if (var_x < still_det->var_threshold &&
		    var_y < still_det->var_threshold &&
		    var_z < still_det->var_threshold) {
			still_det->mean_x = still_det->acc_x * inv;
			still_det->mean_y = still_det->acc_y * inv;
			still_det->mean_z = still_det->acc_z * inv;
			complete = 1;
		}
		/* Reset and start over */
		still_det_reset(still_det);
	}
	return complete;
}
