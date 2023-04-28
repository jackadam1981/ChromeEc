/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Private sensor interface */

#ifndef __CROS_EC_FPC_PRIVATE_H
#define __CROS_EC_FPC_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** @brief Common results returned by BEP functions.
 *
 * \par BEP config/usage errors:
 * <b>Examples:</b> Incorrect arguments/parameters when calling BEP API
 * functions; functions called in incorrect order. <br>
 * <b>Action:</b> Fix SW bug. <br>
 * \par
 * FPC_BEP_RESULT_GENERAL_ERROR <br>
 * FPC_BEP_RESULT_NOT_IMPLEMENTED <br>
 * FPC_BEP_RESULT_NOT_SUPPORTED <br>
 * FPC_BEP_RESULT_NOT_INITIALIZED <br>
 * FPC_BEP_RESULT_CANCELLED <br>
 * FPC_BEP_RESULT_NO_RESOURCE <br>
 * FPC_BEP_RESULT_WRONG_STATE <br>
 * FPC_BEP_RESULT_ID_NOT_UNIQUE <br>
 * FPC_BEP_RESULT_ID_NOT_FOUND <br>
 * FPC_BEP_RESULT_INVALID_FORMAT <br>
 * FPC_BEP_RESULT_INVALID_ARGUMENT <br>
 * FPC_BEP_RESULT_INVALID_PARAMETER <br>
 * FPC_BEP_RESULT_INVALID_CALIBRATION <br>
 * FPC_BEP_RESULT_MISSING_TEMPLATE <br>
 * FPC_BEP_RESULT_STORAGE_NOT_FORMATTED <br>
 * FPC_BEP_RESULT_SENSOR_NOT_INITIALIZED <br>
 * FPC_BEP_RESULT_SENSOR_MISMATCH <br>
 * FPC_BEP_RESULT_CRYPTO_ERROR
 *
 * \par Dynamic memory/heap errors:
 * <b>Examples:</b> Memory leak; heap is too small. <br>
 * <b>Action:</b> Fix SW bug. <br>
 * \par
 * FPC_BEP_RESULT_NO_MEMORY
 *
 * \par Sensor and communication errors:
 * <b>Examples:</b> Broken sensor communication lines; unstable power supply. <br>
 * <b>Action:</b> Fix HW bug. <br>
 * \par
 * FPC_BEP_RESULT_BROKEN_SENSOR <br>
 * FPC_BEP_RESULT_INTERNAL_ERROR <br>
 * FPC_BEP_RESULT_TIMEOUT <br>
 * FPC_BEP_RESULT_IO_ERROR
 *
 * \par Image capture errors:
 * <b>Examples:</b> Finger removed from sensor too quickly. <br>
 * <b>Action:</b> Call the function again. <br>
 * \par
 * FPC_BEP_RESULT_IMAGE_CAPTURE_ERROR
 */

typedef enum {
    /** No errors occurred. */
    FPC_BEP_RESULT_OK = 0,
    /** General error. */
    FPC_BEP_RESULT_GENERAL_ERROR = -1,
    /** Internal error. */
    FPC_BEP_RESULT_INTERNAL_ERROR = -2,
    /** Invalid argument. */
    FPC_BEP_RESULT_INVALID_ARGUMENT = -3,
    /** The functionality is not implemented. */
    FPC_BEP_RESULT_NOT_IMPLEMENTED = -4,
    /** The operation was cancelled. */
    FPC_BEP_RESULT_CANCELLED = -5,
    /** Out of memory. */
    FPC_BEP_RESULT_NO_MEMORY = -6,
    /** Resources are not available. */
    FPC_BEP_RESULT_NO_RESOURCE = -7,
    /** An I/O error occurred. */
    FPC_BEP_RESULT_IO_ERROR = -8,
    /** Sensor is broken. */
    FPC_BEP_RESULT_BROKEN_SENSOR = -9,
    /** The operation cannot be performed in the current state. */
    FPC_BEP_RESULT_WRONG_STATE = -10,
    /** The operation timed out. */
    FPC_BEP_RESULT_TIMEOUT = -11,
    /** The ID is not unique. */
    FPC_BEP_RESULT_ID_NOT_UNIQUE = -12,
    /** The ID is not found. */
    FPC_BEP_RESULT_ID_NOT_FOUND = -13,
    /** The format is invalid. */
    FPC_BEP_RESULT_INVALID_FORMAT = -14,
    /** An image capture error occurred. */
    FPC_BEP_RESULT_IMAGE_CAPTURE_ERROR = -15,
    /** Sensor hardware id or sensor configuration mismatch. */
    FPC_BEP_RESULT_SENSOR_MISMATCH = -16,
    /** Invalid parameter. */
    FPC_BEP_RESULT_INVALID_PARAMETER = -17,
    /** Missing Template. */
    FPC_BEP_RESULT_MISSING_TEMPLATE = -18,
    /** Invalid Calibration.*/
    FPC_BEP_RESULT_INVALID_CALIBRATION = -19,
    /** Calibration/template storage not formatted.*/
    FPC_BEP_RESULT_STORAGE_NOT_FORMATTED = -20,
    /** Sensor hasn't been initialized. */
    FPC_BEP_RESULT_SENSOR_NOT_INITIALIZED = -21,
    /** Enroll fail after too many bad images. */
    FPC_BEP_RESULT_TOO_MANY_BAD_IMAGES = -22,
    /** Cryptographic operation failed. */
    FPC_BEP_RESULT_CRYPTO_ERROR = -23,
    /** The functionality is not supported. */
    FPC_BEP_RESULT_NOT_SUPPORTED = -24,
    /** Finger not stable. */
    FPC_BEP_RESULT_FINGER_NOT_STABLE = -25,
    /** The functionality could not be used before it's initialized. */
    FPC_BEP_RESULT_NOT_INITIALIZED = -26,
} fpc_bep_result_t;

typedef struct {
	uint32_t num_defective_pixels;
} fp_sensor_info_t;

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
			  fp_sensor_info_t *fp_sensor_info);

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

#endif /* __CROS_EC_FPC_PRIVATE_H */
