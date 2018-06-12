/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common code for OCTOPUS_SENSOR_* options */

#include "adc.h"
#include "config.h"
#include "thermistor.h"
#include "util.h"

/*
 * Data derived from Seinhart-Hart equation in a resistor divider circuit with
 * Vdd=3300mV, R = 51.1Kohm, and Murata NCP15WB-series thermistor (B = 4050,
 * T0 = 298.15, nominal resistance (R0) = 47Kohm).
 */
#ifdef OCTOPUS_SENSOR_NCP15WB_51_47
#define THERMISTOR_SCALING_FACTOR_51_47 11
static const struct thermistor_data_pair thermistor_data_51_47[] = {
	{ 2512 / THERMISTOR_SCALING_FACTOR_51_47, 0 },
	{ 2158 / THERMISTOR_SCALING_FACTOR_51_47, 10 },
	{ 1772 / THERMISTOR_SCALING_FACTOR_51_47, 20 },
	{ 1398 / THERMISTOR_SCALING_FACTOR_51_47, 30 },
	{ 1070 / THERMISTOR_SCALING_FACTOR_51_47, 40 },
	{  803 / THERMISTOR_SCALING_FACTOR_51_47, 50 },
	{  597 / THERMISTOR_SCALING_FACTOR_51_47, 60 },
	{  443 / THERMISTOR_SCALING_FACTOR_51_47, 70 },
	{  329 / THERMISTOR_SCALING_FACTOR_51_47, 80 },
	{  285 / THERMISTOR_SCALING_FACTOR_51_47, 85 },
	{  247 / THERMISTOR_SCALING_FACTOR_51_47, 90 },
	{  214 / THERMISTOR_SCALING_FACTOR_51_47, 95 },
	{  187 / THERMISTOR_SCALING_FACTOR_51_47, 100 },
};

static const struct thermistor_info thermistor_info_51_47 = {
	.scaling_factor = THERMISTOR_SCALING_FACTOR_51_47,
	.num_pairs = ARRAY_SIZE(thermistor_data_51_47),
	.data = thermistor_data_51_47,
};

int get_ncp15wb_51_47_temp(int adc, int *temp_ptr)
{
	int mv = adc_read_channel(adc);

	if (mv < 0)
		return EC_ERROR_UNKNOWN;

	*temp_ptr = thermistor_linear_interpolate(mv, &thermistor_info_51_47);
	*temp_ptr = C_TO_K(*temp_ptr);
	return EC_SUCCESS;
}
#endif /* OCTOPUS_SENSOR_NCP15WB_51_47 */

/*
 * Data derived from Seinhart-Hart equation in a resistor divider circuit with
 * Vdd=3300mV, R = 13.7Kohm, and Murata NCP15WB-series thermistor (B = 4050,
 * T0 = 298.15, nominal resistance (R0) = 47Kohm).
 */
#ifdef OCTOPUS_SENSOR_NCP15WB_13_47
#define THERMISTOR_SCALING_FACTOR_13_47 13
static const struct thermistor_data_pair thermistor_data_13_47[] = {
	{ 3044 / THERMISTOR_SCALING_FACTOR_13_47, 0 },
	{ 2890 / THERMISTOR_SCALING_FACTOR_13_47, 10 },
	{ 2680 / THERMISTOR_SCALING_FACTOR_13_47, 20 },
	{ 2418 / THERMISTOR_SCALING_FACTOR_13_47, 30 },
	{ 2117 / THERMISTOR_SCALING_FACTOR_13_47, 40 },
	{ 1800 / THERMISTOR_SCALING_FACTOR_13_47, 50 },
	{ 1490 / THERMISTOR_SCALING_FACTOR_13_47, 60 },
	{ 1208 / THERMISTOR_SCALING_FACTOR_13_47, 70 },
	{  966 / THERMISTOR_SCALING_FACTOR_13_47, 80 },
	{  860 / THERMISTOR_SCALING_FACTOR_13_47, 85 },
	{  766 / THERMISTOR_SCALING_FACTOR_13_47, 90 },
	{  679 / THERMISTOR_SCALING_FACTOR_13_47, 95 },
	{  603 / THERMISTOR_SCALING_FACTOR_13_47, 100 },
};

static const struct thermistor_info thermistor_info_13_47 = {
	.scaling_factor = THERMISTOR_SCALING_FACTOR_13_47,
	.num_pairs = ARRAY_SIZE(thermistor_data_13_47),
	.data = thermistor_data_13_47,
};

int get_ncp15wb_13_47_temp(int adc, int *temp_ptr)
{
	int mv = adc_read_channel(adc);

	if (mv < 0)
		return EC_ERROR_UNKNOWN;

	*temp_ptr = thermistor_linear_interpolate(mv, &thermistor_info_13_47);
	*temp_ptr = C_TO_K(*temp_ptr);
	return EC_SUCCESS;
}
#endif /* OCTOPUS_SENSOR_NCP15WB_13_47 */
