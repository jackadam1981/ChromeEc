/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NCP15WB temperature sensor module for Chrome EC */

#ifndef __CROS_EC_TEMP_SENSOR_EC_ADC_H
#define __CROS_EC_TEMP_SENSOR_EC_ADC_H

/* Some EC has it's own ADC modules, define here EC's max ADC channels.
 * We can consider every channel as a thermal sensor. 
 *  */

enum ec_own_adc_channel {
	THERMISTOR_ADC_CHANNEL_NONE      = -1,
	THERMISTOR_ADC_CHANNEL_0 = 0,
	THERMISTOR_ADC_CHANNEL_1 = 1,
	THERMISTOR_ADC_CHANNEL_2 = 2,
	THERMISTOR_ADC_CHANNEL_3 = 3,
	THERMISTOR_ADC_CHANNEL_4 = 4,
	THERMISTOR_ADC_CHANNEL_COUNT     = 5,
};

/**
 * Get the latest value from the sensor.
 *
 * @param idx		ADC channel to read.
 * @param temp_ptr	Destination for temperature in K.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int thermistor_get_val(int idx, int *temp_ptr);

#endif  /* __CROS_EC_TEMP_SENSOR_NCP15WB_H */
