/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_DRIVER_H_
#define __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_DRIVER_H_

#include "common.h"
#include "fpsensor.h"

/* TODO: THIS SHOULD PROBABLY BE IN A CONFIG FILE OR SOMETHING */
#if defined(HAVE_PRIVATE)
#define HAVE_FP_PRIVATE_DRIVER
#endif

#if defined(HAVE_PRIVATE) && !defined(EMU_BUILD) && defined(SECTION_IS_RW)

#if defined(CONFIG_FP_SENSOR_ELAN80) || defined(CONFIG_FP_SENSOR_ELAN515)
#include "elan/elan_sensor.h"
#define FP_SENSOR_IMAGE_SIZE (FP_SENSOR_IMAGE_SIZE_ELAN)
#define FP_SENSOR_RES_X (FP_SENSOR_RES_X_ELAN)
#define FP_SENSOR_RES_Y (FP_SENSOR_RES_Y_ELAN)
#define FP_ALGORITHM_TEMPLATE_SIZE (FP_ALGORITHM_TEMPLATE_SIZE_ELAN)
#define FP_MAX_FINGER_COUNT (FP_MAX_FINGER_COUNT_ELAN)

#elif defined(CONFIG_FP_SENSOR_FPC1025) || \
	defined(CONFIG_FP_SENSOR_FPC1035) || defined(CONFIG_FP_SENSOR_FPC1145)
#include "fpc/fpc_sensor.h"
#define FP_SENSOR_IMAGE_SIZE (FP_SENSOR_IMAGE_SIZE_FPC)
#define FP_SENSOR_RES_X (FP_SENSOR_RES_X_FPC)
#define FP_SENSOR_RES_Y (FP_SENSOR_RES_Y_FPC)
#define FP_ALGORITHM_TEMPLATE_SIZE (FP_ALGORITHM_TEMPLATE_SIZE_FPC)
#define FP_MAX_FINGER_COUNT (FP_MAX_FINGER_COUNT_FPC)

#else
#error Sensor type not defined!
#endif
#else
/* These values are used by the host (emulator) tests. */
#define FP_SENSOR_IMAGE_SIZE 0
#define FP_SENSOR_RES_X 0
#define FP_SENSOR_RES_Y 0
#define FP_ALGORITHM_TEMPLATE_SIZE 0
#define FP_MAX_FINGER_COUNT 5
#endif

struct fp_sensor_interface *get_fp_sensor_driver(void);

#endif /* __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_DRIVER_H_ */
