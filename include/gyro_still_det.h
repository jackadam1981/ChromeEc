/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_GYRO_STILL_DET_H
#define __CROS_EC_GYRO_STILL_DET_H

#include "common.h"
#include "math_util.h"
#include "stdbool.h"

struct gyro_still_det {
	/**
	 * Variance threshold for the stillness confidence score.
	 * [sensor units]^2
	 */
	fp_t var_threshold;

	/**
	 * Delta about the variance threshold for calculation of the stillness
	 * confidence score [0,1]. [sensor units]^2
	 */
	fp_t confidence_delta;

	/**
	 * Flag to indicate when enough samples have been collected for
	 * a complete stillness calculation.
	 */
	bool stillness_window_ready;

	/**
	 * Flag to signal the beginning of a new stillness detection window.
	 * This is used to keep track of the window start time.
	 */
	bool start_new_window;

	/** Starting time stamp for the current window. */
	uint32_t window_start_time;

	/**
	 * Accumulator variables for tracking the sample mean during
	 * the stillness period.
	 */
	uint32_t num_acc_samples;
	fp_t mean_x, mean_y, mean_z;

	/**
	 * Accumulator variables for computing the window sample mean and
	 * variance for the current window (used for stillness detection).
	 */
	uint32_t num_acc_win_samples;
	fp_t win_mean_x, win_mean_y, win_mean_z;
	fp_t assumed_mean_x, assumed_mean_y, assumed_mean_z;
	fp_t acc_var_x, acc_var_y, acc_var_z;

	/** Stillness period mean (used for look-ahead). */
	fp_t prev_mean_x, prev_mean_y, prev_mean_z;

	/** Latest computed variance. */
	fp_t win_var_x, win_var_y, win_var_z;

	/**
	 * Stillness confidence score for current and previous sample
	 * windows [0,1] (used for look-ahead).
	 */
	fp_t stillness_confidence;
	fp_t prev_stillness_confidence;

	/** Timestamp of last sample recorded. */
	uint32_t last_sample_time;
};

/** Update the stillness detector with a new sample. */
void gyro_still_det_update(struct gyro_still_det *gyro_still_det,
			   uint32_t stillness_win_endtime, uint32_t sample_time,
			   fp_t x, fp_t y, fp_t z);

/** Calculates and returns the stillness confidence score [0,1]. */
fp_t gyro_still_det_compute(struct gyro_still_det *gyro_still_det);

/**
 * Resets the stillness detector and initiates a new detection window.
 *
 * @param reset_stats Determines whether the stillness statistics are reset.
 */
void gyro_still_det_reset(struct gyro_still_det *gyro_still_det,
			  bool reset_stats);

#endif /* __CROS_EC_GYRO_STILL_DET_H */
