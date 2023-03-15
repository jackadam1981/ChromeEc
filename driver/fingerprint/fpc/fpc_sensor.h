/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_FINGERPRINT_FPC_FPC_SENSOR_H_
#define __CROS_EC_DRIVER_FINGERPRINT_FPC_FPC_SENSOR_H_

#include "common.h"
#include "fpsensor.h"
#include "fpsensor_types.h"

#if defined(CONFIG_FP_SENSOR_FPC1025) || defined(CONFIG_FP_SENSOR_FPC1035)
#include "bep/fpc_bep_private.h"
#elif defined(CONFIG_FP_SENSOR_FPC1145)
#include "libfp/fpc_libfp_private.h"
#elif !defined(TEST_BUILD)
#error "Sensor type not defined!"
#endif

int fpc_fp_maintenance(uint16_t *error_state);

/**
 * Returns the FPC sensor driver structure
 *
 * @return fp_sensor_interface
 */
struct fp_sensor_interface *fpc_sensor_get_interface(void);

#endif /* __CROS_EC_DRIVER_FINGERPRINT_FPC_FPC_SENSOR_H_ */
