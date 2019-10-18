/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "newton_fit.h"
#include "math.h"
#include "math_util.h"
#include <string.h>

#ifndef CONFIG_FPU
#error "Newton fit requires CONFIG_FPU"
#endif

static float distance_squared(floatv3_t a, floatv3_t b)
{
	floatv3_t delta;

	fpv3_init(delta, a[X] - b[X], a[Y] - b[Y], a[Z] - b[Z]);
	return fpv3_dot(delta, delta);
}

static float compute_error(struct newton_fit *fit, floatv3_t center)
{
	float error = 0.0f;
	uint32_t i;

	for (i = 0; i < fit->state->num_orientations; ++i) {
		float e = 1.0f - distance_squared(fit->orientations[i], center);

		error += e * e;
	}
	return error;

}

void newton_fit_reset(struct newton_fit *fit)
{
	memset((void *) fit->state, 0, sizeof(struct newton_fit_state));
}

int newton_fit_accumulate(struct newton_fit *fit, float x, float y, float z)
{
	uint32_t i;
	floatv3_t v, delta;

	fpv3_init(v, x, y, z);

	for (i = 0; i < fit->state->num_orientations; ++i) {
		fpv3_sub(delta, v, fit->orientations[i]);
		if (fpv3_dot(delta, delta) >= fit->config->nearness_threshold)
			continue;

		fpv3_scalar_mul(fit->orientations[i],
				1.0f - fit->config->new_pt_weight);
		fpv3_scalar_mul(v, fit->config->new_pt_weight);
		fpv3_add(fit->orientations[i], fit->orientations[i], v);
		return 1;
	}

	if (fit->state->num_orientations < fit->config->num_orientations) {
		fpv3_init(fit->orientations[fit->state->num_orientations],
			  x, y, z);
		fit->state->num_orientations++;
		return 1;
	}

	return 0;
}

void newton_fit_compute(struct newton_fit *fit, floatv3_t bias, float *radius)
{
	floatv3_t new_bias, offset, delta;
	fp_t error, new_error;
	uint32_t iteration = 0, i;

	memcpy(new_bias, bias, sizeof(floatv3_t));
	new_error = compute_error(fit, new_bias);

	do {
		memcpy(bias, new_bias, sizeof(floatv3_t));
		error = new_error;
		fpv3_init(offset, 0.0f, 0.0f, 0.0f);

		for (i = 0; i < fit->state->num_orientations; ++i) {
			float mag;

			fpv3_sub(delta, fit->orientations[i], bias);
			mag = fpv3_dot(delta, delta);
			if (mag <= 0.0f)
				/* Something went wrong, orientation ignored. */
				continue;
			mag = sqrtf(mag);
			fpv3_scalar_mul(delta, (mag - 1.0f) / mag);
			fpv3_add(offset, offset, delta);
		}

		fpv3_scalar_mul(offset, 1.0f / fit->state->num_orientations);
		fpv3_add(new_bias, bias, offset);
		new_error = compute_error(fit, new_bias);
		if (new_error > error)
			memcpy(new_bias, bias, sizeof(floatv3_t));
		++iteration;
	} while (iteration < fit->config->max_iterations &&
		 new_error < error &&
		 new_error > fit->config->error_threshold);

	memcpy(bias, new_bias, sizeof(floatv3_t));

	if (radius) {
		*radius = 0.0f;
		for (i = 0; i < fit->state->num_orientations; ++i) {
			float mag;

			fpv3_sub(delta, fit->orientations[i], bias);
			mag = fpv3_dot(delta, delta);
			if (mag <= 0.0f)
				/* Something went wrong, orientation ignored. */
				continue;
			*radius += sqrtf(mag);
		}
		*radius /= fit->state->num_orientations;
	}
}
