/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Newton's method for sphere fit algorithm */

#ifndef __CROS_EC_NEWTON_FIT_H
#define __CROS_EC_NEWTON_FIT_H

#include "vec3.h"

struct newton_fit_config {
	const fp_t nearness_threshold;
	const fp_t new_pt_weight;
	const fp_t error_threshold;
	const uint32_t num_orientations;
	const uint32_t max_iterations;
};

struct newton_fit_state {
	uint32_t num_orientations;
};

struct newton_fit {
	struct newton_fit_config const *config;
	struct newton_fit_state volatile * state;
	fpv3_t *orientations;
};

#define NEWTON_FIT(SIZE, NEAR_THRES, NEW_PT_WEIGHT, ERROR_THRESHOLD, \
		   MAX_ITERATIONS) \
	((struct newton_fit) { \
		.config = &((struct newton_fit_config){ \
			.nearness_threshold = NEAR_THRES, \
			.new_pt_weight = NEW_PT_WEIGHT, \
			.error_threshold = ERROR_THRESHOLD, \
			.num_orientations = SIZE, \
			.max_iterations = MAX_ITERATIONS, \
		}), \
		.state = &((struct newton_fit_state){}), \
		.orientations = (fpv3_t *) &((fpv3_t[SIZE]) {}), \
	})

/**
 * Reset the newton_fit struct's state.
 *
 * @param fit Pointer to the struct.
 */
void newton_fit_reset(struct newton_fit *fit);

/**
 * Add new vector to the struct. The behavior of this depends on the
 * configuration values used when the struct was created. For example:
 * - Samples that are within sqrt(NEAR_THRES) of an existing orientation will
 *   be averaged with the matching orientation entry.
 * - If the new sample isn't near an existing orientation it will only be added
 *   if state->num_orientations < config->num_orientations.
 *
 * @param fit Pointer to the struct.
 * @param x The new samples' X component.
 * @param y The new samples' Y component.
 * @param z The new samples' Z component.
 * @return 1 if the sample was added, 0 otherwise.
 */
int newton_fit_accumulate(struct newton_fit *fit, fp_t x, fp_t y, fp_t z);

/**
 * Compute the center/bias and optionally the radius represented by the current
 * struct.
 *
 * @param fit Pointer to the struct.
 * @param bias Pointer to the output bias (this is also the starting bias for
 *             the algorithm.
 * @param radius Optional pointer to write the computed radius into. If NULL,
 *               the calculation will be skipped.
 */
void newton_fit_compute(struct newton_fit *fit, fpv3_t bias, fp_t *radius);

#endif /* __CROS_EC_NEWTON_FIT_H */
