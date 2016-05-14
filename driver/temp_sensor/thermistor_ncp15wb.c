/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NCP15WB thermistor module for Chrome EC */

#include "common.h"
#include "thermistor.h"
#include "util.h"

/*
 * ADC-to-temp conversion assumes recommended thermistor / resistor
 * configuration (NCP15WB* / 24.9K) with a 10-bit ADC.
 * For 50C through 100C, use linear interpolation from discreet points
 * in table below. For temps < 50C, use a simplified linear function.
 */
#define ADC_DISCREET_RANGE_START_TEMP	50
/* 10 bit ADC result corresponding to START_TEMP */
#define ADC_DISCREET_RANGE_START_RESULT	407

#define ADC_DISCREET_RANGE_LIMIT_TEMP	100
/* 10 bit ADC result corresponding to LIMIT_TEMP */
#define ADC_DISCREET_RANGE_LIMIT_RESULT	107

/* Table entries in steppings of 5C */
#define ADC_DISCREET_RANGE_STEP		5

/* Discreet range ADC results (9 bit) per temperature, in 5 degree steps */
static const uint8_t adc_result[] = {
	203,	/* 50 C */
	178,	/* 55 C */
	157,	/* 60 C */
	138,	/* 65 C */
	121,	/* 70 C */
	106,	/* 75 C */
	93,	/* 80 C */
	81,	/* 85 C */
	70,	/* 90 C */
	61,	/* 95 C */
	53,	/* 100 C */
};

/*
 * From 20C (reasonable lower limit of temperatures we care about accuracy)
 * to 50C, the temperature curve is roughly linear, so we don't need to include
 * data points in our table.
 */
#define adc_to_temp(result) (ADC_DISCREET_RANGE_START_TEMP - \
	(((result) - ADC_DISCREET_RANGE_START_RESULT) * 3 + 16) / 32)

/* Convert ADC result (10 bit) to temperature in celsius */
int ncp15wb_calculate_temp(uint16_t adc)
{
	int temp;
	int head, tail, mid;
	uint8_t delta, step;

	/* Is ADC result in linear range? */
	if (adc >= ADC_DISCREET_RANGE_START_RESULT)
		temp = adc_to_temp(adc);
	/* Hotter than our discreet range limit? */
	else if (adc <= ADC_DISCREET_RANGE_LIMIT_RESULT)
		temp = ADC_DISCREET_RANGE_LIMIT_TEMP;
	/* We're in the discreet range */
	else {
		/* Table uses 9 bit ADC values */
		adc /= 2;

		/* Binary search to find proper table entry */
		head = 0;
		tail = ARRAY_SIZE(adc_result) - 1;
		while (head != tail) {
			mid = (head + tail) / 2;
			if (adc_result[mid] >= adc &&
			    adc_result[mid+1] < adc)
				break;
			if (adc_result[mid] > adc)
				head = mid + 1;
			else
				tail = mid;
		}

		/* Now fit between table entries using linear interpolation. */
		if (head != tail) {
			delta = adc_result[mid] - adc_result[mid + 1];
			step = ((adc_result[mid] - adc) *
				ADC_DISCREET_RANGE_STEP + delta / 2) / delta;
		} else {
			/* Edge case where adc = max */
			mid = head;
			step = 0;
		}

		temp = ADC_DISCREET_RANGE_START_TEMP +
		       ADC_DISCREET_RANGE_STEP * mid + step;
	}

	return temp;
}

int thermistor_linear_interpolate(uint16_t mv,
		const struct thermistor_info *info)
{
	int i;
	const struct thermistor_data_pair *data = info->data;
	int approx_temp;

	if (mv < data[0].mv * info->scaling_factor)
		return -1;	/* too low */
	else if (mv > data[info->num_pairs - 1].mv * info->scaling_factor)
		return -2;	/* too high */

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
