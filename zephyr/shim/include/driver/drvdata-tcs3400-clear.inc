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
#define DT_DRV_COMPAT			cros_ec_drvdata_tcs3400_clear

#define ALS_CALIBRATION_CHANNEL_SCALE(id)				  \
	.als_cal.channel_scale.k_channel_scale =		  	  \
		  ALS_CHANNEL_SCALE(DT_PROP(id, k_channel_scale)),	  \
	.als_cal.channel_scale.cover_scale =				  \
		  ALS_CHANNEL_SCALE(DT_PROP(id, cover_scale)),

#define ALS_CALIBRATION_SET(id)					  \
		.als_cal.scale = DT_PROP(id, scale),		  \
		.als_cal.uscale = DT_PROP(id, uscale),		  \
		.als_cal.offset = DT_PROP(id, offset),		  \
		ALS_CALIBRATION_CHANNEL_SCALE(DT_CHILD(id, als_channel_scale))

#define ALS_CALIBRATION(id)						  \
		ALS_CALIBRATION_SET(DT_CHILD(id, als_cal))

#define SENSOR_DATA_TCS3400_CLEAR(n)					  \
	static struct als_drv_data_t SENSOR_DATA_NAME(DT_DRV_INST(n)) = { \
		ALS_CALIBRATION(DT_CHILD(DT_DRV_INST(n), als_drv_data))	  \
	};

/* Create driver data for ALS sensors */
DT_INST_FOREACH_STATUS_OKAY(SENSOR_DATA_TCS3400_CLEAR)
