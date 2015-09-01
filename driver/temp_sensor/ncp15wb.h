/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NCP15WB temperature sensor module for Chrome EC */

#ifndef __CROS_EC_TEMP_SENSOR_NCP15WB_H
#define __CROS_EC_TEMP_SENSOR_NCP15WB_H

/**
 * Get the latest value from the sensor.
 *
 * @param adc	10bit raw data on adc.
 *
 * @return temperature in K.
 */
int ncp15wb_calculate_temp(uint16_t adc);

#endif  /* __CROS_EC_TEMP_SENSOR_NCP15WB_H */
