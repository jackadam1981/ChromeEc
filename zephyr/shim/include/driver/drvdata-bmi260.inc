/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file included in "sensor-drvdata-list.h" is, in the end, included in
 * sensors.c to add the declaration of the sensor driver data.
 */

#include "driver/accelgyro_bmi_common_public.h"

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT			cros_ec_drvdata_bmi260

#define SENSOR_DATA_BMI260(n)					\
	static struct bmi_drv_data_t SENSOR_DATA_NAME(DT_DRV_INST(n));

/* Create driver data for BMI260 sensors */
DT_INST_FOREACH_STATUS_OKAY(SENSOR_DATA_BMI260)
