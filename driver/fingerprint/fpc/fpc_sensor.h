/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_FINGERPRINT_FPC_FPC_SENSOR_H_
#define __CROS_EC_DRIVER_FINGERPRINT_FPC_FPC_SENSOR_H_

#include "common.h"
#include "fpc_private.h"
#include "fpsensor_types.h"

#include <stdint.h>

#if defined(CONFIG_FP_SENSOR_FPC1025)
#include "bep/fpc1025_private.h"
#elif defined(CONFIG_FP_SENSOR_FPC1035)
#include "bep/fpc1035_private.h"
#elif defined(CONFIG_FP_SENSOR_FPC1145)
#include "libfp/fpc1145_private.h"
#else
#error "Sensor type not defined!"
#endif

int fpc_fp_maintenance(uint16_t *error_state);

/**
 * Configure finger detection.
 *
 * Send the settings to the sensor, so it is properly configured to detect
 * the presence of a finger.
 */
void fp_sensor_configure_detect(void);

/**
 * Returns the status of the finger on the sensor.
 * (assumes fp_sensor_configure_detect was called before)
 *
 * @return finger_state
 */
enum finger_state fp_sensor_finger_status(void);

/**
 * Returns the FPC sensor driver structure
 *
 * @return fp_sensor_interface
 */
struct fp_sensor_interface *fp_driver_get_fpc(void);

#endif /* __CROS_EC_DRIVER_FINGERPRINT_FPC_FPC_SENSOR_H_ */
