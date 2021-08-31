/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */



#include "temp_sensor/zthermistor.h"
#include "adc.h"
#include "device.h"

#include <stdio.h> // for debugging
#define THERMISTOR_DEVICE_NODE		DT_NODELABEL(thermy)

int get_temp(int idx_adc, int *temp_ptr)
{
	int mv;
	mv = adc_read_channel(idx_adc);
	if (mv < 0)
		return EC_ERROR_UNKNOWN;

	const struct device *thermister_dev =
		DEVICE_DT_GET(THERMISTOR_DEVICE_NODE);

	printf("The num pairs is %d!!!\n", DT_PROP(THERMISTOR_DEVICE_NODE, num_pairs));
	// printf(THERMISTOR_DEVICE_NODE);
	return 0;
}
