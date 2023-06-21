/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "filter_iir.h"

#include <math.h>
#include <stddef.h>

#define DEBUG

/* Console output macros */
#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ##args)

int filter_iir_init(struct filt_iir_t *filter, uint8_t rank, float *a, float *b)
{
	uint8_t i;

	if (rank > FILTER_RANK_MAX)
		return -1;

	filter->rank = rank;
	for (i = 0; i < rank + 1; i++) {
		filter->params[i].x = 0.0f;
		filter->params[i].y = 0.0f;
		filter->params[i].a = a[i];
		filter->params[i].b = b[i];
	}

	return 0;
}

float filter_iir_step(struct filt_iir_t *filter, float x)
{
	uint8_t i;
	float val = 0.0f;

	/* Shift x and y values */
	for (i = (filter->rank); i > 0; i--) {
		filter->params[i].x = filter->params[i - 1].x;
		filter->params[i].y = filter->params[i - 1].y;
	}
	filter->params[0].x = x;
	filter->params[0].y = 0.0f;

	for (i = 0; i < filter->rank + 1; i++) {
		val += filter->params[i].b * filter->params[i].x;
		val -= filter->params[i].a * filter->params[i].y;
	}

	val = val / filter->params[0].a;
	filter->params[0].y = val;

	return val;
}

/*
 * Tangens approximation.
 *
 * Use Taylor series up to a second non-zero coefficient.
 *
 *                           (x - tan(0))^3
 * tan(x) ~=  (x - tan(0)) + --------------
 *                                 3
 */
static float filter_tan_approx(float x)
{
	return (x + x * x * x / 3.0f);
}

/*
 * Convolution of two vectors.
 *
 * This operation is used to calculate dot product of two polynomials.
 */
static void filter_conv(float *in1, float *in2, int len1, int len2, float *out)
{
	int i, j;
	float tab[FILTER_RANK_MAX + 1] = { 0 };

	for (i = 0; i < len1; i++) {
		for (j = 0; j < len2; j++) {
			tab[i + j] += in1[i] * in2[j];

#ifdef DEBUG
			CPRINTS("BD: [%d](%d) * [%d](%d) = [%d](%d)", i,
				(int)(1000000 * in1[i]), j,
				(int)(1000000 * in2[j]), j + i,
				(int)(1000000 * tab[i + j]));
#endif
		}
	}
	for (i = 0; i < len1 + len2 - 1; i++)
		out[i] = tab[i];
}

/*
 * Helper function to return actors of Laplace transformation for
 * the Butterworth filter.
 *
 * Transformation of the filter can be expressed as a product of
 * transformations of 2nd order filters. Each sub-filter is in the form:
 *                 1
 *  Hk(s) = --------------------
 *            (s^2 + As^1 + 1)
 *
 * This function returns parameter A depending on filter RANK and sub-filter
 * number K.
 *
 * If the filter rank is odd there is no need to store additional sub-filter
 * coefficient as it is known to be
 *          1
 * H(s) = -----
 *        s + 1
 *
 * Refer to https://en.wikipedia.org/wiki/Butterworth_filter
 */
static float filter_butterworth_factor(uint8_t rank, uint8_t k)
{
	static const int16_t coef_tab[] = FILTER_BUTTERWORTH_FACTORS;
	int cnt = 0, i;

	for (i = 2; i <= (rank - 1); i++)
		cnt += i / 2;

	return coef_tab[cnt + k] * FILTER_BUTTERWORTH_FACTOR_SCALE;
}

/*
 * Calculate Butterworth filter coefficients.
 *
 * Although the function is optimized to be fast and to have low code footprint
 * it shall not be used frequently as it call substantial amount of float
 * multiplications.
 */
static int butterworth_lpf_create(uint8_t rank, float *b, float *a,
				  float freq_lowpass)
{
	float bfilt[3], afilt[3];
	float W;
	int k;
	int len = 1;

	if (rank > FILTER_RANK_MAX)
		return -ENOMEM;

	W = filter_tan_approx(freq_lowpass * (float)M_PI / 2.0f);
	b[0] = a[0] = 1;

	for (k = 0; k < rank / 2; k++) {
		float coef, dw;

		coef = filter_butterworth_factor(rank, k);
#ifdef DEBUG
		CPRINTS("FILTER: 1M*coef = %d", (int)(1000000 * coef));
#endif

		dw = W * W + coef * W + 1.0f;

		bfilt[0] = W * W / dw;
		bfilt[1] = 2 * bfilt[0];
		bfilt[2] = bfilt[0];

		afilt[0] = 1.0f;
		afilt[1] = (2 * W * W - 2) / dw;
		afilt[2] = (W * W - coef * W + 1) / dw;

		{
			int a;

			for (a = 0; a < 3; a++)
				CPRINTS("%d b[] = %d, a[] = %d", a,
					(int)(1000000 * bfilt[a]),
					(int)(1000000 * afilt[a]));
		}

		filter_conv(a, afilt, len, 3, a);
		filter_conv(b, bfilt, len, 3, b);
		len += 2;
	}
	if (rank & 1) {
		float dw;

		dw = 1 + W;
		bfilt[1] = bfilt[0] = W / dw;
		afilt[0] = 1.0f;
		afilt[1] = (W - 1) / dw;

		filter_conv(a, afilt, rank, 2, a);
		filter_conv(b, bfilt, rank, 2, b);
	}

	return 0;
}

/*
 * Initialize Butterworh low pass filter.
 * freq_lowpass parameter is normalized to the Nyquist frequency.
 */
int filter_butterworth_lpf_init(struct filt_iir_t *filter, uint8_t rank,
				float freq_lowpass)
{
	uint8_t i;
	int ret;
	float b[FILTER_RANK_MAX + 1] = { 0 };
	float a[FILTER_RANK_MAX + 1] = { 0 };

	if (rank > FILTER_RANK_MAX)
		return -1;

	ret = butterworth_lpf_create(rank, b, a, freq_lowpass);
	if (ret != 0)
		return ret;

	filter->rank = rank;
	for (i = 0; i < rank + 1; i++) {
		filter->params[i].x = 0.0f;
		filter->params[i].y = 0.0f;
		filter->params[i].a = a[i];
		filter->params[i].b = b[i];

#ifdef DEBUG
		CPRINTS("FILTER: 1M*b[%d] = %d, 1M*a[%d] = %d", i,
			(int)(1000000 * b[i]), i, (int)(1000000 * a[i]));
#endif
	}

	return 0;
}
