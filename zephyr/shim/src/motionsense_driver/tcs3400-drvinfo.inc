/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * CREATE_MOTION_SENSOR is the macro to create an entry in
 * motion_sensors array. The macro gets value of compatible property of
 * the sensor in device tree. It creates the corresponding
 * motion_sense_t entry for motion_sensors array.
 */

/*
 * Here, we call CREATE_MOTION_SENSOR for each TCS3400_clear
 * instance (compatible = "cros-ec,drv-tcs3400-clear")
 * in device tree to create motion_sensor_t entry for the instance.
 */
CREATE_MOTION_SENSOR(cros_ec_drv_tcs3400_clear, MOTIONSENSE_CHIP_TCS3400, \
			MOTIONSENSE_TYPE_LIGHT, tcs3400_drv,		  \
			TCS3400_LIGHT_MIN_FREQ, TCS3400_LIGHT_MAX_FREQ)

/*
 * Here, we call CREATE_MOTION_SENSOR for each TCS3400_rgb
 * instance (compatible = "cros-ec,drv-tcs3400-rgb")
 * in device tree to create motion_sensor_t entry for the instance.
 */
CREATE_MOTION_SENSOR(cros_ec_drv_tcs3400_rgb, MOTIONSENSE_CHIP_TCS3400,	\
			MOTIONSENSE_TYPE_LIGHT_RGB, tcs3400_rgb_drv, 0, 0)
