/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STILLNESS_DETECTOR_H
#define __CROS_EC_STILLNESS_DETECTOR_H

#include <stdint.h>

struct still_det {
	/** Variance threshold for the stillness confidence score. [units]^2 */
	float var_threshold;

	/** The minimum window duration to consider a still sample. */
	uint32_t min_batch_window;

	/** The maximum window duration to consider a still sample. */
	uint32_t max_batch_window;

	/**
	 * The minimum number of samples in a window to consider a still sample.
	 */
	uint32_t min_batch_size;

	/** The timestamp of the first sample in the current batch. */
	uint32_t window_start_time;

	/** The number of samples in the current batch. */
	uint32_t num_samples;

	/** Accumulators used for calculating stillness. */
	float acc_x, acc_y, acc_z,
	      acc_xx, acc_yy, acc_zz,
	      mean_x, mean_y, mean_z;
};

/**
 * Initialize a stillness detector.
 *
 * @param still_dat Pointer to the stillness detector to initialize.
 * @param var_threshold The variance threshold to set.
 * @param min_batch_window The minimum batch window to set (us).
 * @param max_batch_window The maximum batch window to set (us).
 * @param min_batch_size The minimum batch size to set.
 */
void still_det_init(struct still_det *still_det, float var_threshold,
		    uint32_t min_batch_window, uint32_t max_batch_window,
		    uint32_t min_batch_size);

/**
 * Update a stillness detector with a new sample.
 *
 * @param sample_time The timestamp of the sample to add.
 * @param x The x component of the sample to add.
 * @param y The y component of the sample to add.
 * @param z The z component of the sample to add.
 * @return 1 if the sample triggered a complete batch and mean_* are now valid.
 */
int still_det_update(struct still_det *still_det, uint32_t sample_time,
		     float x, float y, float z);

#endif /* __CROS_EC_STILLNESS_DETECTOR_H */
