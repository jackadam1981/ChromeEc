/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file included in "sensor-drvdata-list.h" is, in the end, included in
 * sensors.c to add the declaration of the sensor driver data.
 */

#include "driver/als_tcs3400_public.h"

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT			cros_ec_drvdata_tcs3400_rgb

#define	TCS3400_RGB_CAL_RGB_SET_SCALE(id)				\
	.scale = {							\
		.k_channel_scale =					\
			ALS_CHANNEL_SCALE(DT_PROP(id, k_channel_scale)),\
		.cover_scale =						\
			ALS_CHANNEL_SCALE(DT_PROP(id, cover_scale)),	\
	},

#define TCS3400_RGB_CAL_RGB_SET_ONE(id, suffix)				\
	.calibration.rgb_cal[suffix] = {				\
	    .offset = DT_PROP(id, offset),				\
	    .coeff[0] = FLOAT_TO_FP(DT_PROP_BY_IDX(id, coeff, 0)),	\
	    .coeff[1] = FLOAT_TO_FP(DT_PROP_BY_IDX(id, coeff, 1)),	\
	    .coeff[2] = FLOAT_TO_FP(DT_PROP_BY_IDX(id, coeff, 2)),	\
	    .coeff[3] = FLOAT_TO_FP(DT_PROP_BY_IDX(id, coeff, 3)),	\
	    TCS3400_RGB_CAL_RGB_SET_SCALE(DT_CHILD(id, scale))		\
	},

#define TCS3400_RGB_CAL(id)						\
		TCS3400_RGB_CAL_RGB_SET_ONE(DT_CHILD(id, rgb_cal_x), X)	\
		TCS3400_RGB_CAL_RGB_SET_ONE(DT_CHILD(id, rgb_cal_y), Y)	\
		TCS3400_RGB_CAL_RGB_SET_ONE(DT_CHILD(id, rgb_cal_z), Z)	\
		.calibration.irt = INT_TO_FP(DT_PROP(id, irt)),

#define TCS3400_RGB_SATRURATION(id)					\
		COND_CODE_1(DT_NODE_HAS_PROP(id, again),		\
			(.saturation.again = DT_PROP(id, again),),	\
			(.saturation.again = TCS_DEFAULT_AGAIN,))	\
		COND_CODE_1(DT_NODE_HAS_PROP(id, atime),		\
			(.saturation.again = DT_PROP(id, atime),),	\
			(.saturation.again = TCS_DEFAULT_ATIME,))
		
#define SENSOR_DATA_TCS3400_RGB(n)					      \
	static struct tcs3400_rgb_drv_data_t				      \
			SENSOR_DATA_NAME(DT_DRV_INST(n)) = {		      \
		TCS3400_RGB_CAL(DT_CHILD(DT_DRV_INST(n), calibration))	      \
		TCS3400_RGB_SATRURATION(DT_CHILD(DT_DRV_INST(n), saturation)) \
	};

/* Create driver data for TCS3400_RGB sensors */
DT_INST_FOREACH_STATUS_OKAY(SENSOR_DATA_TCS3400_RGB)
