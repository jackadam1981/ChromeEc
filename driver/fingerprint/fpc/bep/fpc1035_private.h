/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FPC1035_PRIVATE_H
#define __CROS_EC_FPC1035_PRIVATE_H

#include "fpsensor_types.h"

/*
 * Constant value corresponding to the maximum template size
 * for FPC1035 sensor. Client template memory allocation must
 * have this size. This includes extra memory for template update.
 *
 * Template size + alignment padding + size of template size variable
 */
#define FPC1035_ALGORITHM_TEMPLATE_SIZE (14373 + 3 + 4)
#define FPC1035_ALGORITHM_ENROLLMENT_SIZE_FPC (4)

/*
 * Sensor image size
 *
 * Value from fpc_bep_image_get_buffer_size(): (112*88)+660
 */
#define FPC1035_SENSOR_IMAGE_SIZE (10516)

struct fpsensor_config bep_sensor = {
	.hwid = 0x011,
	.name = "FPC1035",
	.res_x = 112,
	.res_y = 88,
	.res_bpp = 8,
	.image_size = FPC1035_SENSOR_IMAGE_SIZE,
	.real_image_size = 112 * 88,
	.image_offset = 400,
	.algorithm_enrollment_size = FP_ALGORITHM_ENROLLMENT_SIZE_FPC,
	.algorithm_template_size = 14373 + 3 + 4,
	.max_finger_count = 5,
};

#endif /* __CROS_EC_FPC1035_PRIVATE_H */
