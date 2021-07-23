/* Copyright 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Temperature sensor module for LM4 chip */

#ifndef __CROS_EC_TEMP_SENSOR_CHIP_H
#define __CROS_EC_TEMP_SENSOR_CHIP_H

/**
 * Get the last polled value of the sensor.
 *
 * @param idx		Sensor index to read.
 * @param temp_k_ptr	Destination for temperature in K.
 * @param temp_mk_ptr	Optional destination for temperature in mK.
 *			Leave NULL if not required.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 *	   EC_ERROR_INVAL if temp_mk_ptr not NULL and mK is not supported.
 */
int chip_temp_sensor_get_val(int idx,  int *temp_k_ptr, int *temp_mk_ptr);

#endif /* __CROS_EC_TEMP_SENSOR_CHIP_H */
