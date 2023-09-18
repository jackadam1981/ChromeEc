/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_SHIM_INCLUDE_FPSENSOR_DRIVER_H__
#define __ZEPHYR_SHIM_INCLUDE_FPSENSOR_DRIVER_H__

#include <drivers/cros_fingerprint.h>

#define FP_SENSOR_IMAGE_OFFSET CONFIG_FP_SENSOR_IMAGE_OFFSET
#define FP_SENSOR_IMAGE_SIZE CONFIG_FP_SENSOR_IMAGE_SIZE
#define FP_SENSOR_RES_X FINGERPRINT_SENSOR_RES_X(DT_CHOSEN(cros_fp_fingerprint_sensor))
#define FP_SENSOR_RES_Y FINGERPRINT_SENSOR_RES_Y(DT_CHOSEN(cros_fp_fingerprint_sensor))
#define FP_ALGORITHM_TEMPLATE_SIZE CONFIG_FP_ALGORITHM_TEMPLATE_SIZE
#define FP_MAX_FINGER_COUNT CONFIG_FP_MAX_FINGER_COUNT

/*
 * Tell fpsensor code that private driver is present, even if this is a public
 * build. If the build is public, we will provide mocks.
 */
#define HAVE_FP_PRIVATE_DRIVER

#endif /* __ZEPHYR_SHIM_INCLUDE_FPSENSOR_DRIVER_H__ */
