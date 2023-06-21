/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FILTER_H
#define __CROS_EC_FILTER_H

#include "common.h"

struct smooth_exp_t {
	float x;
	float a;
	float a_complementary;
	bool is_valid;
};

/*
 * Exponential smoothing
 * y(n+1) = a*y(n) + (1-a)*x(n)
 */
static inline void smooth_exp_init(struct smooth_exp_t *exp, float a, float x0)
{
	exp->a = a;
	exp->a_complementary = 1.0f - a;
	exp->x = x0;
}

static inline float smooth_exp_step(struct smooth_exp_t *exp, float x)
{
	float ret;

	ret = exp->a * exp->x + exp->a_complementary * x;
	exp->x = ret;

	return ret;
}

/*
 * Infinite Impulse Response filter Nth rank
 *
 *        b(0) + b(1)z^(-1) + b(2)z^(-2) + ..... + b(n)z^(-(n-1))
 * Y(z) = ---------------------------------------------------------- X(z)
 *        a(0) + a(1)z^(-1) + a(2)z^(-2) + ..... + a(n)z^(-(n-1))
 *
 * a(0)*y(0) =     sum     { b(i)*x(k-i) } -     sum      {a(i)*y(k-i) }
 *             k=0...(n-1)                     k=1...(n-1)
 */

/*
 * Maximum filter rank supported.
 */
#define FILTER_RANK_MAX 5
/*
 * Factors of Butterworth filter up to FILTER_RANK_MAX.
 * Transformation of the filter can be expressed as a product of
 * transformations of 2nd order filters. Each sub-filter is in the form:
 *                 1
 *  H(s) = --------------------
 *          (s^2 + As^1 + 1)
 * The A coefficients have to be provided below as int16_t fixed precision
 * expressed in 0.0001f units.
 *
 * All coefficients have to be placed in order of N:K, where N is rank and K
 * is the iterator of sub-filter. For 5th order the format is:
 * {2:1,  3:1,  4:1,  4:2,  5:1,  5:2}
 *
 * If the filter rank is odd there is no need to store additional sub-filter
 * coefficient as it is known to be
 *          1
 * H(s) = -----
 *        s + 1
 *
 * Refer to https://en.wikipedia.org/wiki/Butterworth_filter
 */
#define FILTER_BUTTERWORTH_FACTORS                     \
	{                                              \
		14142, 10000, 7654, 18478, 6180, 16180 \
	}
#define FILTER_BUTTERWORTH_FACTOR_SCALE 0.0001f

struct filt_param_t {
	float x;
	float y;
	float a;
	float b;
};

struct filt_iir_t {
	uint8_t rank;
	struct filt_param_t params[FILTER_RANK_MAX + 1];
};

int filter_iir_init(struct filt_iir_t *filter, uint8_t rank, float *a,
		    float *b);
float filter_iir_step(struct filt_iir_t *filter, float x);
int filter_butterworth_lpf_init(struct filt_iir_t *filter, uint8_t rank,
				float freq_lowpass);

#endif
