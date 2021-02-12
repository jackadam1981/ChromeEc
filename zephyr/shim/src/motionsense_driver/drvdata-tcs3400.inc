/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file included by "sensor-drvdata-list.h" is, in the end, included in
 * mostionsense_sensors.c to put the declaration of the sensor driver data.
 */

/* include macros for common data strutures from accelgyro.h */
#include "drvdata-accelgyro.h"

/* Create driver data for tcs3400 driver. */
#define SENSOR_DATA_TCS3400_CLEAR(n, compat)				      \
	static struct als_drv_data_t SENSOR_DATA_NAME(DT_INST(n, compat)) =   \
	    ACCELGYRO_ALS_DRV_DATA(DT_CHILD(DT_INST(n, compat), als_drv_data));

#define CREATE_SENSOR_DATA_TCS3400_CLEAR(compat)			\
	UTIL_LISTIFY(DT_NUM_INST_STATUS_OKAY(compat),			\
		SENSOR_DATA_TCS3400_CLEAR, compat)

/*
 * Create driver data for each TCS3400-clear drvdata instance in device tree.
 * (compatible = "cros-ec,drvdata-tcs3400-clear")
 */
CREATE_SENSOR_DATA_TCS3400_CLEAR(cros_ec_drvdata_tcs3400_clear)

/* driver data for tcs3400 rgb */
#define TCS3400_RGB_SATRURATION(id)					\
		COND_CODE_1(DT_NODE_HAS_PROP(id, again),		\
			(.saturation.again = DT_PROP(id, again),),	\
			(.saturation.again = TCS_DEFAULT_AGAIN,))	\
		COND_CODE_1(DT_NODE_HAS_PROP(id, atime),		\
			(.saturation.again = DT_PROP(id, atime),),	\
			(.saturation.again = TCS_DEFAULT_ATIME,))

#define SENSOR_DATA_TCS3400_RGB(n, compat)				\
	static struct tcs3400_rgb_drv_data_t				\
			SENSOR_DATA_NAME(DT_INST(n, compat)) = {	\
		.calibration = ACCELGYRO_RGB_CALIBRATION(		\
			DT_CHILD(DT_INST(n, compat), rgb_calibration)),	\
		TCS3400_RGB_SATRURATION(DT_CHILD(DT_INST(n, compat),	\
			saturation))					\
	};

#define CREATE_SENSOR_DATA_TCS3400_RGB(compat)				\
	UTIL_LISTIFY(DT_NUM_INST_STATUS_OKAY(compat),			\
		SENSOR_DATA_TCS3400_RGB, compat)
/*
 * Create driver data for each TCS3400-rgb drvdata instance in device tree.
 * (compatible = "cros-ec,drvdata-tcs3400-rgb")
 */
CREATE_SENSOR_DATA_TCS3400_RGB(cros_ec_drvdata_tcs3400_rgb)
