/* Copyright 2017 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FPC1145_PRIVATE_H
#define __CROS_EC_FPC1145_PRIVATE_H

#include "fpsensor_types.h"

#include <stdint.h>

#define FPC1145_ALGO

/* Acquired finger frame definitions */
#define FPC1145_SENSOR_IMAGE_SIZE_MODE_VENDOR_FPC (35460)
#define FPC1145_SENSOR_IMAGE_SIZE_MODE_SIMPLE_FPC (13356)

/*
 * Size of the captured image in MQT mode. If you this is modified the
 * corresponding value in the MQT tool fputils.py must be changed too.
 * See b/111443750 for context.
 */
#define FPC1145_SENSOR_IMAGE_SIZE_MODE_QUAL_FPC (24408)

/* Opaque FPC context */
#define FP_SENSOR_CONTEXT_SIZE_FPC 4944

/* Algorithm buffer sizes */
#define FPC1145_ALGORITHM_ENROLLMENT_SIZE 28
#define FPC1145_ALGORITHM_TEMPLATE_SIZE (47552)
#define FPC1145_SENSOR_IMAGE_SIZE FPC1145_SENSOR_IMAGE_SIZE_MODE_VENDOR_FPC
#define FPC1145_SENSOR_IMAGE_OFFSET_FPC (2340)
#define FPC1145_SENSOR_RES_X (56)
#define FPC1145_SENSOR_RES_Y (192)
#define FPC1145_SENSOR_RES_BPP (8)

static const struct fpsensor_config fpc1145_config = {
	.hwid = 0x140,
	.name = "FPC1145",
	.res_x = FPC1145_SENSOR_RES_X,
	.res_y = FPC1145_SENSOR_RES_Y,
	.res_bpp = 8,

	.image_size = FPC1145_SENSOR_IMAGE_SIZE,
	.real_image_size = FPC1145_SENSOR_RES_X * FPC1145_SENSOR_RES_Y,
	.image_offset = 2340,
	.algorithm_enrollment_size = 28,
	.algorithm_template_size = 47552,
	.max_finger_count = 5,
};

#endif /* __CROS_EC_FPC1145_PRIVATE_H */
