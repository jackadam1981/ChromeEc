/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/als_tcs3410_public.h"

/*
 * CREATE_SENSOR_DATA which is defined in motionsense_sensros.c is
 * the helper to create sensor driver specific data.
 *
 * CREATE_SENSOR_DATA gets two arguments. One is the compatible
 * property value specified in device tree and the other one is the macro
 * that actually creates sensor driver specific data. The macro gets
 * node id and the name to be used for the sensor driver data.
 */

/* include macros for common data strutures from accelgyro.h */
#include "drvdata-accelgyro.h"

/*
 * Create driver data for each TCS3410-rgb drvdata instance in device tree.
 * (compatible = "cros-ec,drvdata-tcs3410-rgb")
 */
CREATE_SENSOR_DATA(cros_ec_drvdata_tcs3410_rgb, CREATE_SENSOR_DATA_TCS3410_RGB)
/*
 * CREATE_MOTION_SENSOR which is defined in motionsense_sensros.c is
 * the macro to create an entry in motion_sensors array.
 * The macro gets value of compatible property of
 * the sensor in device tree and sensor specific values like chip ID,
 * type of sensor, name of driver, default min/max frequency.
 * Then using the values, it creates the corresponding motion_sense_t entry
 * in motion_sensors array.
 */

/*
 * Here, we call CREATE_MOTION_SENSOR to create a motion_sensor_t entry
 * for each TCS3410 clear instance (compatible = "cros-ec,tcs3410-clear")
 * in device tree.
 */
CREATE_MOTION_SENSOR(cros_ec_tcs3410_clear, MOTIONSENSE_CHIP_TCS3410,	\
		MOTIONSENSE_TYPE_LIGHT, tcs3410_drv,			\
		TCS3410_LIGHT_MIN_FREQ, TCS3410_LIGHT_MAX_FREQ)

/*
 * Here, we call CREATE_MOTION_SENSOR to create a motion_sensor_t entry
 * for each TCS3410 RGB instance (compatible = "cros-ec,tcs3410-rgb")
 * in device tree.
 */
CREATE_MOTION_SENSOR(cros_ec_tcs3410_rgb, MOTIONSENSE_CHIP_TCS3410,	\
		MOTIONSENSE_TYPE_LIGHT_RGB, tcs3410_rgb_drv, 0, 0)
