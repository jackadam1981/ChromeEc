/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NCP15WB temperature sensor module for Chrome EC */

#ifndef __CROS_EC_TEMP_SENSOR_NCP15WB_H
#define __CROS_EC_TEMP_SENSOR_NCP15WB_H

/* Some EC has it's own ADC modules, define here EC's max ADC channels.
 * We can consider every channel as a thermal sensor.
 *  */
#ifdef CONFIG_TEMP_SENSOR_MEC1322_OWN
/* MEC1322 ADC channels */
enum ec_own_adc_channel {
	OWN_ADC_CHANNEL_NONE      = -1,
	MEC1322_ADC_CHANNEL_0 = 0,
	MEC1322_ADC_CHANNEL_1 = 1,
	MEC1322_ADC_CHANNEL_2 = 2,
	MEC1322_ADC_CHANNEL_3 = 3,
	MEC1322_ADC_CHANNEL_4 = 4,
	OWN_ADC_CHANNEL_COUNT     = 5,
};
#else
enum ec_own_adc_channel {
	OWN_ADC_CHANNEL_NONE = -1,
	OWN_ADC_CHANNEL_COUNT     =  0,
};
#endif

/**
 * Get the latest value from the sensor.
 *
 * @param adc	10bit raw data on adc.
 *
 * @return temperature in K.
 */
int ncp15wb_calculate_temp(uint16_t adc);

/**
 * Get the latest value from the sensor.
 *
 * @param idx		ADC channel to read.
 * @param temp_ptr	Destination for temperature in K.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int ncp15wb_get_val(int idx, int *temp_ptr);

#endif  /* __CROS_EC_TEMP_SENSOR_NCP15WB_H */
