/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_FINGERPRINT_EGIS_EGIS_API_H_
#define __CROS_EC_DRIVER_FINGERPRINT_EGIS_EGIS_API_H_

#include "plat_log.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (defined(CONFIG_FP_SENSOR_EGIS630))
#define FP_SENSOR_HWID_EGIS 630
#define FP_SENSOR_RES_X_EGIS 80
#define FP_SENSOR_RES_Y_EGIS 64
#define FP_SENSOR_IMAGE_SIZE_EGIS (FP_SENSOR_RES_X_EGIS * FP_SENSOR_RES_Y_EGIS)
#define FP_ALGORITHM_TEMPLATE_SIZE_EGIS (10 * 1024)
#define FP_MAX_FINGER_COUNT_EGIS 1
#else
#define FP_SENSOR_HWID_EGIS 600
#define FP_SENSOR_RES_X_EGIS 0
#define FP_SENSOR_RES_Y_EGIS 0
#define FP_SENSOR_IMAGE_SIZE_EGIS \
	(FP_SENSOR_RES_X_EGIS * FP_SENSOR_RES_Y_EGIS * sizeof(uint16_t))
#define FP_ALGORITHM_TEMPLATE_SIZE_EGIS (0)
#define FP_MAX_FINGER_COUNT_EGIS 0
#endif

#define FP_SENSOR_IMAGE_OFFSET_EGIS (0)
#define FP_SENSOR_RES_BPP_EGIS (8)

typedef enum {
	EGIS_API_OK,
	EGIS_API_WAIT_EVENT_FINGER_PRESENT,
	EGIS_API_CAPTURE_DONE,
	EGIS_API_ENABLE_EVENT_FINGER_PRESENT,
	EGIS_API_WAIT_TIME,
	EGIS_API_FINGER_PRESENT,
	EGIS_API_FINGER_LOST,
	EGIS_API_FINGER_UNSTABLE,
	EGIS_API_FINGER_PARTIAL,
	EGIS_API_CALIBRATION_INTERRUPT,
	EGIS_API_ERROR_TOO_FAST,
	EGIS_API_ERROR_TOO_SLOW,
	EGIS_API_ERROR_GENERAL,
	EGIS_API_ERROR_SENSOR,
	EGIS_API_ERROR_MEMORY,
	EGIS_API_ERROR_PARAMETER,
	EGIS_API_FAIL_LOW_QUALITY,
	EGIS_API_FAIL_IDENTIFY_START,
	EGIS_API_FAIL_IDENTIFY_IMAGE,
	EGIS_API_ERROR_INVALID_FINGERID,
	EGIS_API_ERROR_OUT_RECORD,

	EGIS_API_ERROR_SENSOR_NEED_RESET = 99,
	EGIS_API_ERROR_SENSOR_OCP_DETECT = 110,
} egis_api_return_t;

/**
 * @brief Reset and initialize the sensor IC.
 *
 * @return 0 on success.
 * @return positive value on error.
 */
int egis_sensor_init(void);

/**
 * Deinitialize the sensor IC.
 *
 * @return 0 on success.
 * @return positive value on error.
 */
int egis_sensor_deinit(void);

/**
 * Power down the sensor IC.
 *
 */
void egis_sensor_power_down(void);

/**
 * Acquire a fingerprint image with specific capture mode.
 *
 * @param[out] image_data Image from sensor. Buffer must be allocated by caller
 * with size FP_SENSOR_IMAGE_SIZE.
 * @param mode  enum fp_capture_type.
 *
 * @return 0 on success.
 * @return positive value on error.
 */
int egis_get_image_with_mode(uint8_t *image_data, int mode);

/**
 * Get 8bits image data from EGIS fingerprint sensor.
 *
 * @param[out] image_data Image from sensor. Buffer must be allocated by caller
 * with size FP_SENSOR_IMAGE_SIZE.
 *
 * @return 0 on success.
 * @return positive value on error.
 */
int egis_get_image(uint8_t *image_data);

/**
 * Set the finger detection mode for the Egis sensor.
 *
 */
void egis_set_detect_mode(void);

/**
 * Check the sensor interrupt status.
 *
 * @return 0 on success.
 * @return EGIS_API_FINGER_PRESENT when the finger is present.
 * @return EGIS_API_FINGER_LOST when the finger is partial.
 * @return other positive value on error.
 */
int egis_check_int_status(void);

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_DRIVER_FINGERPRINT_EGIS_EGIS_API_H_ */
