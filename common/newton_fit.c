/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "newton_fit.h"
#include "math.h"
#include "math_util.h"
#include <string.h>

#define CPRINTS(fmt, args...) cprints(CC_MOTION_SENSE, fmt, ##args)

static float distance_squared(floatv3_t a, floatv3_t b)
{
	floatv3_t delta;

	fpv3_init(delta, a[X] - b[X], a[Y] - b[Y], a[Z] - b[Z]);
	return fpv3_dot(delta, delta);
}

static float compute_error(struct newton_fit *fit, floatv3_t center)
{
	float error = 0.0f;
	struct queue_iterator it;
	struct newton_fit_orientation *_it;

	for (queue_begin(fit->orientations, &it); it.ptr != NULL;
	     queue_next(fit->orientations, &it)) {
		float e;

		_it = (struct newton_fit_orientation *)it.ptr;
		e = 1.0f - distance_squared(_it->orientation, center);
		error += e * e;
	}

	return error;
}

static bool is_ready_to_compute(struct newton_fit *fit, bool prune)
{
	bool has_min_samples = true;
	struct queue_iterator it;
	struct newton_fit_orientation *_it;

	/* Not full, not ready to compute. */
	if (!queue_is_full(fit->orientations)) {
		CPRINTS("queue not full (size=%d), not ready to compute",
			(int)queue_count(fit->orientations));
		return false;
	}

	/* Inspect all the orientations. */
	for (queue_begin(fit->orientations, &it); it.ptr != NULL;
	     queue_next(fit->orientations, &it)) {
		_it = (struct newton_fit_orientation *)it.ptr;
		/* If an orientation has too few samples, flag that. */
		CPRINTS("    orientation %u/%u", _it->nsamples,
			fit->min_orientation_samples);
		if (_it->nsamples < fit->min_orientation_samples) {
			has_min_samples = false;
			break;
		}
	}

	/* If all orientations have the minimum samples, we're done and can
	 * compute the bias.
	 */
	if (has_min_samples)
		return true;

	/* If we got here and prune is true, then we need to remove the oldest
	 * entry to make room for new orientations.
	 */
	if (prune)
		queue_advance_head(fit->orientations, 1);

	return false;
}

void newton_fit_reset(struct newton_fit *fit)
{
	queue_init(fit->orientations);
}

bool newton_fit_accumulate(struct newton_fit *fit, float x, float y, float z)
{
	struct queue_iterator it;
	struct newton_fit_orientation *_it;
	floatv3_t v, delta;

	fpv3_init(v, x, y, z);

	/* Check if we can merge this new data point with an existing
	 * orientation.
	 */
	for (queue_begin(fit->orientations, &it); it.ptr != NULL;
	     queue_next(fit->orientations, &it)) {
		_it = (struct newton_fit_orientation *)it.ptr;

		fpv3_sub(delta, v, _it->orientation);
		/* Skip entries that are too far away. */
		if (fpv3_dot(delta, delta) >= fit->nearness_threshold)
			continue;

		/* Merge new data point with this orientation. */
		fpv3_scalar_mul(_it->orientation, 1.0f - fit->new_pt_weight);
		fpv3_scalar_mul(v, fit->new_pt_weight);
		fpv3_add(_it->orientation, _it->orientation, v);
		if (_it->nsamples < 0xff)
			_it->nsamples++;
		return is_ready_to_compute(fit, false);
	}

	/* If queue isn't full. */
	if (!queue_is_full(fit->orientations)) {
		struct newton_fit_orientation entry;

		entry.nsamples = 1;
		fpv3_init(entry.orientation, x, y, z);
		queue_add_unit(fit->orientations, &entry);

		return is_ready_to_compute(fit, false);
	}

	return is_ready_to_compute(fit, true);
}

void newton_fit_compute(struct newton_fit *fit, floatv3_t bias, float *radius)
{
	struct queue_iterator it;
	struct newton_fit_orientation *_it;
	floatv3_t new_bias, offset, delta;
	fp_t error, new_error;
	uint32_t iteration = 0;
	float inv_orient_count;

	if (queue_is_empty(fit->orientations))
		return;

	inv_orient_count = 1.0f / queue_count(fit->orientations);

	memcpy(new_bias, bias, sizeof(floatv3_t));
	new_error = compute_error(fit, new_bias);

	do {
		memcpy(bias, new_bias, sizeof(floatv3_t));
		error = new_error;
		fpv3_init(offset, 0.0f, 0.0f, 0.0f);

		for (queue_begin(fit->orientations, &it); it.ptr != NULL;
		     queue_next(fit->orientations, &it)) {
			float mag;

			_it = (struct newton_fit_orientation *)it.ptr;

			fpv3_sub(delta, _it->orientation, bias);
			mag = fpv3_dot(delta, delta);
			if (mag <= 0.0f)
				/* Something went wrong, orientation ignored. */
				continue;
			mag = sqrtf(mag);
			fpv3_scalar_mul(delta, (mag - 1.0f) / mag);
			fpv3_add(offset, offset, delta);
		}

		fpv3_scalar_mul(offset, inv_orient_count);
		fpv3_add(new_bias, bias, offset);
		new_error = compute_error(fit, new_bias);
		if (new_error > error)
			memcpy(new_bias, bias, sizeof(floatv3_t));
		++iteration;
	} while (iteration < fit->max_iterations && new_error < error &&
		 new_error > fit->error_threshold);

	memcpy(bias, new_bias, sizeof(floatv3_t));

	if (radius) {
		*radius = 0.0f;
		for (queue_begin(fit->orientations, &it); it.ptr != NULL;
		     queue_next(fit->orientations, &it)) {
			float mag;

			_it = (struct newton_fit_orientation *)it.ptr;
			fpv3_sub(delta, _it->orientation, bias);
			mag = fpv3_dot(delta, delta);
			if (mag <= 0.0f)
				/* Something went wrong, orientation ignored. */
				continue;
			*radius += sqrtf(mag);
		}
		*radius *= inv_orient_count;
	}
}
