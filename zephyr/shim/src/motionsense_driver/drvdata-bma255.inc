/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file included by "sensor-drvdata-list.h" is, in the end, included in
 * mostionsense_sensors.c to put the declaration of the sensor driver data.
 */

/* Create driver data for bma255 driver. */
#define SENSOR_DATA_BMA255(inst, compat)				\
	static struct accelgyro_saved_data_t				\
		SENSOR_DATA_NAME(DT_INST(inst, compat));

#define CREATE_SENSOR_DATA_BMA255(compat)				\
	UTIL_LISTIFY(DT_NUM_INST_STATUS_OKAY(compat), SENSOR_DATA_BMA255, compat)

/*
 * Create driver data for each BMI260 drvinfo instance in device tree.
 * (compatible = "cros-ec,drvdata-bma255")
 */
CREATE_SENSOR_DATA_BMA255(cros_ec_drvdata_bma255)
