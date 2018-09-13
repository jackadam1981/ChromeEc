/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "math.h"
#include "math_util.h"
#include "vec3.h"
#include "util.h"

/* TODO(b:113364863): Replace all float* types with fixed-point type fp_t. */
void floatv3_scalar_mul(floatv3_t v, float c)
{
	v[X] *= c;
	v[Y] *= c;
	v[Z] *= c;
}

float floatv3_dot(const floatv3_t v, const floatv3_t w)
{
	return v[X] * w[X] + v[Y] * w[Y] + v[Z] * w[Z];
}

float floatv3_norm_squared(const floatv3_t v)
{
	return floatv3_dot(v, v);
}

float floatv3_norm(const floatv3_t v)
{
	return sqrtf(floatv3_norm_squared(v));
}

void fpv3_scalar_mul(fpv3_t v, fp_t c)
{
	v[X] = fp_mul(v[X], c);
	v[Y] = fp_mul(v[Y], c);
	v[Z] = fp_mul(v[Z], c);
}

fp_t fpv3_dot(const fpv3_t v, const fpv3_t w)
{
	return fp_mul(v[X], w[X]) + fp_mul(v[Y], w[Y]) + fp_mul(v[Z], w[Z]);
}

fp_t fpv3_norm_squared(const fpv3_t v)
{
	return fpv3_dot(v, v);
}

fp_t fpv3_norm(const fpv3_t v)
{
	return fp_sqrtf(fpv3_norm_squared(v));
}
