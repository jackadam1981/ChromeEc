/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FPC1025_PRIVATE_H
#define __CROS_EC_FPC1025_PRIVATE_H

#include "fpsensor_types.h"

/*
 * Constant value corresponding to the maximum template size
 * for FPC1025 sensor. Client template memory allocation must
 * have this size. This includes extra memory for template update.
 *
 * Template size + alignment padding + size of template size variable
 */
#define FPC1025_ALGORITHM_TEMPLATE_SIZE (5088 + 0 + 4)

/*
 * Sensor image size
 *
 * Value from fpc_bep_image_get_buffer_size(): (160*160)+660
 */
#define FPC1025_SENSOR_IMAGE_SIZE (26260)

#define FPC1025_ALGORITHM_ENROLLMENT_SIZE (4)

static const struct fpsensor_config bep_sensor = {
	.hwid = 0x021,
	.name = "FPC1025",
	.res_x = 160, /**< sensor width */
	.res_y = 160, /**< sensor height */
	.res_bpp = 8, /**< resolution bits per pixel */
	.image_size = FPC1025_SENSOR_IMAGE_SIZE,
	.real_image_size = 160 * 160,
	.image_offset = 400,
	.algorithm_enrollment_size = FPC1025_ALGORITHM_ENROLLMENT_SIZE,
	.algorithm_template_size = 5088 + 0 + 4,
	.max_finger_count = 5,
};

#endif /* __CROS_EC_FPC1025_PRIVATE_H */
