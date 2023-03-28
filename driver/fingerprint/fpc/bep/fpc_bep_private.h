/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Private sensor interface */

#ifndef __CROS_EC_DRIVER_FINGERPRINT_FPC_LIBFP_FPC_BEP_PRIVATE_H
#define __CROS_EC_DRIVER_FINGERPRINT_FPC_LIBFP_FPC_BEP_PRIVATE_H

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "fpc_sensor.h"
#include "fpsensor_types.h"

#include <stdint.h>

/* Max number of templates stored / matched against */
#define FP_MAX_FINGER_COUNT_FPC (5)

#if defined(CONFIG_FP_SENSOR_FPC1025)
#include "fpc1025_private.h"
#define FP_SENSOR_IMAGE_SIZE_FPC FPC1025_SENSOR_IMAGE_SIZE
#define FP_ALGORITHM_TEMPLATE_SIZE_FPC FPC1025_ALGORITHM_TEMPLATE_SIZE
#define FP_ALGORITHM_ENROLLMENT_SIZE_FPC FPC1025_ALGORITHM_ENROLLMENT_SIZE

#elif defined(CONFIG_FP_SENSOR_FPC1035)

#include "fpc1035_private.h"
#define FP_SENSOR_IMAGE_SIZE_FPC FPC1035_SENSOR_IMAGE_SIZE
#define FP_ALGORITHM_TEMPLATE_SIZE_FPC FPC1035_ALGORITHM_TEMPLATE_SIZE
#define FP_ALGORITHM_ENROLLMENT_SIZE_FPC FPC1035_ALGORITHM_ENROLLMENT_SIZE
#else
#error "No supported sensor"
#endif

#include <stdint.h>

struct fp_sensor_info_t {
	uint32_t num_defective_pixels;
};

/**
 * fp_sensor_maintenance runs a test for defective pixels and should
 * be triggered periodically by the client. Internally, a defective
 * pixel list is maintained and the algorithm will compensate for
 * any defect pixels when matching towards a template.
 *
 * The defective pixel update will abort and return an error if any of
 * the finger detect zones are covered. A client can call
 * fp_sensor_finger_status to determine the current status.
 *
 * @param[in]  image_data      pointer to a buffer containing at least
 * FP_SENSOR_IMAGE_SIZE_FPC bytes of memory
 * @param[out] fp_sensor_info  Structure containing output data.
 *
 * @return
 * - 0 on success
 * - negative value on error
 */
int fp_sensor_maintenance(uint8_t *image_data,
			  struct fp_sensor_info_t *fp_sensor_info);

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
 * Get the HWID of the sensor.
 *
 * @param id Pointer to where to store the HWID value.  The HWID value here is
 * the full 16 bits (contrast to FP_SENSOR_HWID_FPC where the lower four bits,
 * which are a manufacturing id, are truncated).
 * @return
 * - EC_SUCCESS on success
 * - EC_ERROR_INVAL or FP_ERROR_SPI_COMM on error
 */
int fpc_get_hwid(uint16_t *id);

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_DRIVER_FINGERPRINT_FPC_LIBFP_FPC_BEP_PRIVATE_H */
