/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NCP15WB thermistor module for Chrome EC */

#include "common.h"
#include "thermistor.h"
#include "util.h"

int thermistor_linear_interpolate(uint16_t mv,
		const struct thermistor_info *info)
{
	int i, approx_temp;
	const struct thermistor_data_pair *data = info->data;

	/* We need at least two points to form a line. */
	ASSERT(info->num_pairs >= 2);

	/* If input value is out of bounds, return the lowest or highest
	 * value in the data sets provided. */
	if (mv < data[0].mv * info->scaling_factor)
		return data[0].temp;
	else if (mv > data[info->num_pairs - 1].mv * info->scaling_factor)
		return data[info->num_pairs - 1].temp;

	for (i = 0; i < info->num_pairs - 1; i++) {
		int step_mv, num_steps;
		int approx_mv;
		int v0 = data[i].mv * info->scaling_factor;
		int v1 = data[i + 1].mv * info->scaling_factor;

		if ((mv < v0) || (mv > v1))
			continue;

		num_steps = data[i + 1].temp - data[i].temp;
		step_mv = (v1 - v0) / num_steps;

		approx_mv = v0;
		approx_temp = data[i].temp;
		while (approx_mv < mv) {
			/* break if next step will be further away from mv */
			if (approx_mv + step_mv > mv) {
				int d0 = mv - approx_mv;
				int d1 = approx_mv + step_mv - mv;

				if (d0 < d1)
					break;
			}

			approx_mv += step_mv;
			approx_temp++;
		}
	}

	return approx_temp;
}
