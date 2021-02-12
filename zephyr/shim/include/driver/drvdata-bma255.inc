/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file included in "sensor-drvdata-list.h" is, in the end, included in
 * sensors.c to add the declaration of the sensor driver data.
 */

#include "accelgyro.h"

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT			cros_ec_drvdata_bma255

#define SENSOR_DATA_BMA255(n)					\
	static struct accelgyro_saved_data_t SENSOR_DATA_NAME(DT_DRV_INST(n));

/* Create driver data for BMA255 sensors */
DT_INST_FOREACH_STATUS_OKAY(SENSOR_DATA_BMA255)
