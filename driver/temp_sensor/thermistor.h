/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Thermistor module for Chrome EC */

#ifndef __CROS_EC_TEMP_SENSOR_THERMISTOR_H
#define __CROS_EC_TEMP_SENSOR_THERMISTOR_H

struct thermistor_data_pair {
	uint8_t mv;	/* Scaled voltage level at ADC (in mV) */
	uint8_t temp;	/* Temperature in Celsius */
};

struct thermistor_info {
	uint8_t scaling_factor;	/* Scaling factor for voltage in data pair. */
	uint8_t num_pairs;	/* Number of data pairs. */
	/*
	 * Must contain at least two pairs. Values between given data pairs
	 * will be calculated as points on a line. Pairs must be sorted in
	 * ascending order. More pairs gives better accuracy at the expense
	 * of memory, so insert points where the curve changes significantly.
	 */
	struct thermistor_data_pair *data;
};

/**
 * @brief Calculate temperature using linear interpolation of data points.
 *
 * Given a set of datapoints, the algorithm will calculate the "step" in
 * between each one in order to interpolate missing entries. Data points
 * should be derived using Steinhart-Hart equation. Data points should be
 * used after significant changes in slope to increase accuracy.
 *
 * @param mv	Value read from ADC (in millivolts).
 * @param info	Thermistor reference point info.
 *
 * @return	temperature in C, or negative value to indicate error
 */
int thermistor_linear_interpolate(uint16_t mv,
				const struct thermistor_info *info);

/**
 * ncp15wb temperature conversion routine.
 *
 * @param adc	10bit raw data on adc.
 *
 * @return	temperature in C.
 */
int ncp15wb_calculate_temp(uint16_t adc);

#endif  /* __CROS_EC_TEMP_SENSOR_THERMISTOR_NCP15WB_H */
