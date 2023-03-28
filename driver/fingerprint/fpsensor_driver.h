/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_DRIVER_H_
#define __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_DRIVER_H_

#include "fpsensor_limits.h"

#if defined(HAVE_PRIVATE) && !defined(EMU_BUILD)
#define HAVE_FP_PRIVATE_DRIVER

#if defined(CONFIG_FP_SENSOR_ELAN80) || defined(CONFIG_FP_SENSOR_ELAN515)
#include "elan/elan_sensor.h"
#define FP_SENSOR_IMAGE_SIZE (FP_SENSOR_IMAGE_SIZE_ELAN)
#define FP_ALGORITHM_TEMPLATE_SIZE (FP_ALGORITHM_TEMPLATE_SIZE_ELAN)
#define FP_MAX_FINGER_COUNT (FP_MAX_FINGER_COUNT_ELAN)

#elif defined(CONFIG_FP_SENSOR_FPC1025) || \
	defined(CONFIG_FP_SENSOR_FPC1035) || defined(CONFIG_FP_SENSOR_FPC1145)
#include "fpc/fpc_sensor.h"
#define FP_SENSOR_IMAGE_SIZE (FP_SENSOR_IMAGE_SIZE_FPC)
#define FP_ALGORITHM_TEMPLATE_SIZE (FP_ALGORITHM_TEMPLATE_SIZE_FPC)
#define FP_MAX_FINGER_COUNT (FP_MAX_FINGER_COUNT_FPC)
#endif
#else
/* These values are used by the host (emulator) tests. */
#define FP_SENSOR_IMAGE_SIZE 0
#define FP_ALGORITHM_TEMPLATE_SIZE 0
#define FP_MAX_FINGER_COUNT 5
#endif

#if FPSENSOR_MAX_IMAGE_SIZE < FP_SENSOR_IMAGE_SIZE
#error "Insufficient space allocated for sensor image"
#endif

#if FPSENSOR_MAX_ALGORITHM_MAX_TEMPLATE_SIZE < FP_ALGORITHM_TEMPLATE_SIZE
#error "Insufficient space allocated for algorithm template size"
#endif

#if FPSENSOR_MAX_FINGER_COUNT < FP_MAX_FINGER_COUNT
#error "Insufficient space allocated for number of fingers"
#endif

struct fp_sensor_interface *fpsensor_detect_get_driver(void);

#endif /* __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_DRIVER_H_ */
