/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_DRIVER_H_
#define __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_DRIVER_H_

#include "config.h"

#if defined(HAVE_PRIVATE)
#define HAVE_FP_PRIVATE_DRIVER
#endif

#if defined(CONFIG_FP_SENSOR_ELAN80) || defined(CONFIG_FP_SENSOR_ELAN515)
#include "elan/elan_sensor.h"
#define FP_SENSOR_IMAGE_OFFSET (FP_SENSOR_IMAGE_OFFSET_ELAN)
#define FP_SENSOR_IMAGE_SIZE (FP_SENSOR_IMAGE_SIZE_ELAN)
#define FP_ALGORITHM_TEMPLATE_SIZE (FP_ALGORITHM_TEMPLATE_SIZE_ELAN)
#define FP_MAX_FINGER_COUNT (FP_MAX_FINGER_COUNT_ELAN)
#elif defined(CONFIG_FP_SENSOR_FPC1025) || \
	defined(CONFIG_FP_SENSOR_FPC1035) || defined(CONFIG_FP_SENSOR_FPC1145)
#include "fpc/fpc_sensor.h"
#define FP_SENSOR_IMAGE_OFFSET (FP_SENSOR_IMAGE_OFFSET_FPC)
#define FP_SENSOR_IMAGE_SIZE (FP_SENSOR_IMAGE_SIZE_FPC)
#define FP_ALGORITHM_TEMPLATE_SIZE (FP_ALGORITHM_TEMPLATE_SIZE_FPC)
#define FP_MAX_FINGER_COUNT (FP_MAX_FINGER_COUNT_FPC)
#elif defined(TEST_BUILD)
#define FP_SENSOR_IMAGE_OFFSET (0)
#define FP_SENSOR_IMAGE_SIZE (512)
#define FP_ALGORITHM_TEMPLATE_SIZE (512)
#define FP_MAX_FINGER_COUNT (5)
#else
#error "No sensor defined"
#endif

struct fp_sensor_interface *fpsensor_detect_get_driver(void);

#endif /* __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_DRIVER_H_ */
