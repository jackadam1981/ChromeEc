/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_H_
#define __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_H_

#ifdef HAVE_PRIVATE
#define HAVE_FP_PRIVATE_DRIVER
#endif

#ifdef EMU_BUILD
/* These values are used by the host (emulator) tests. */
#define FP_SENSOR_IMAGE_SIZE 0
#define FP_SENSOR_RES_X 0
#define FP_SENSOR_RES_Y 0
#define FP_ALGORITHM_TEMPLATE_SIZE 0
#define FP_MAX_FINGER_COUNT 5
#else
#include "fpc/fpc_info.h"
#endif

#ifdef TEST_BUILD
/* This represents the mock of the private */
#define HAVE_FP_PRIVATE_DRIVER
#endif

#endif /* __CROS_EC_DRIVER_FINGERPRINT_FPSENSOR_H_ */
