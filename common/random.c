/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "math.h"
#include "math_util.h"
#include "random.h"

#define HUGE 2147483647

float rnd_uniform(int *seed)
{
	int k = *seed / 127773;

	*seed = 16807 * (*seed - k * 127773) - k * 2836;
	if (*seed < 0)
		*seed += HUGE;
	return (*seed * 4.656612875e-10f);
}

float rnd_normal_01(int *seed)
{
	float r1 = rnd_uniform(seed);
	float r2 = rnd_uniform(seed);

	return sqrtf(-2.0f * logf(r1)) * cosf(2.0f * M_PI * r2);
}

float rnd_normal(float mean, float stddev, int *seed)
{
	return mean + stddev * rnd_normal_01(seed);
}
