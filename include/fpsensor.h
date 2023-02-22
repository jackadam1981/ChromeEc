/* Copyright 2017 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fingerprint sensor interface */

#ifndef __CROS_EC_FPSENSOR_H
#define __CROS_EC_FPSENSOR_H

#include "common.h"
#include "ec_commands.h"
#include "fpsensor_driver.h"
#include "fpsensor_types.h"
#include "fpsensor_utils.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SPI_FP_DEVICE
#define SPI_FP_DEVICE (&spi_devices[0])
#endif

/*  Four-character-code */
#define FOURCC(a, b, c, d)                                              \
	((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | \
	 ((uint32_t)(d) << 24))

/* 8-bit greyscale pixel format as defined by V4L2 headers */
#define V4L2_PIX_FMT_GREY FOURCC('G', 'R', 'E', 'Y')

/* --- functions provided by the sensor-specific driver --- */

struct fp_sensor_interface {
	/* Whether there is an ELAN fingerprint sensor or
	 * FPC sensor.
	 */
	const enum fp_sensor_type sensor_type;

	/* Hardware-specific sensor identifier */
	const uint32_t sensor_hwid;

	/*
	 * Initialize the connected sensor hardware and put
	 * it in a low power mode.
	 */
	int (*sensor_init)(void);

	/* De-initialize the sensor hardware.
	 * @return 0 on success
	 * @return negative value on error
	 */
	int (*sensor_deinit)(void);

	/*
	 * Fill the 'ec_response_fp_info' buffer with the
	 * sensor information as required by the
	 * EC_CMD_FP_INFO host command.
	 *
	 * Put both the static information and the ones read
	 * from the sensor at runtime.
	 *
	 * @param[out] resp sensor info
	 *
	 * @return EC_SUCCESS on success
	 * @return EC_RES_ERROR on error
	 */
	int (*sensor_get_info)(struct ec_response_fp_info *resp);

	/*
	 * Put the sensor in its lowest power state.
	 *
	 * fp_sensor_configure_detect needs to be called to
	 * restore finger detection functionality.
	 */
	void (*sensor_low_power)(void);

	/*
	 * Configure finger detection.
	 *
	 * Send the settings to the sensor, so it is
	 * properly configured to detect the presence of a
	 * finger.
	 */
	/* TODO(b/184101599): Remove "_" suffix. */
	void (*sensor_configure_detect)(void);

	/*
	 * Returns the status of the finger on the sensor.
	 * (assumes fp_sensor_configure_detect was called
	 * before)
	 */
	/* TODO(b/184101599): Remove "_" suffix. */
	enum finger_state (*sensor_finger_status)(void);

	/*
	 * Acquires a fingerprint image with specific
	 * capture mode.
	 *
	 * Same as the fp_sensor_acquire_image function
	 * above, excepted 'mode' can be set to one of the
	 * FP_CAPTURE_ constants to get a specific image
	 * type (e.g. a pattern) rather than the default
	 * one.
	 */
	/* TODO(b/184101599): Remove "_" suffix. */
	int (*sensor_acquire_image_with_mode_)(uint8_t *image_data, int mode);

	/*
	 * Adds fingerprint image to the current enrollment
	 * session.
	 *
	 * @return a negative value on error or one of the
	 * following codes:
	 * - EC_MKBP_FP_ERR_ENROLL_OK when image was
	 * successfully enrolled
	 * - EC_MKBP_FP_ERR_ENROLL_IMMOBILE when image
	 * added, but user should be advised to move finger
	 * - EC_MKBP_FP_ERR_ENROLL_LOW_QUALITY when image
	 * could not be used due to low image quality
	 * - EC_MKBP_FP_ERR_ENROLL_LOW_COVERAGE when image
	 * could not be used due to finger covering too
	 * little area of the sensor
	 */
	int (*finger_enroll)(uint8_t *image, int *completion);

	/*
	 * Compares given finger image against enrolled
	 * templates.
	 *
	 * The matching algorithm can update the template
	 * with additional biometric data from the image, if
	 * it chooses to do so.
	 *
	 * @param templ a pointer to the array of template
	 * buffers.
	 * @param templ_count the number of buffers in the
	 * array of templates.
	 * @param image the buffer containing the finger
	 * image
	 * @param match_index index of the matched finger in
	 * the template array if any.
	 * @param update_bitmap contains one bit per
	 * template, the bit is set if the match has updated
	 * the given template.
	 * @return negative value on error, else one of the
	 * following code :
	 * - EC_MKBP_FP_ERR_MATCH_NO on non-match
	 * - EC_MKBP_FP_ERR_MATCH_YES for match when
	 * template was not updated with new data
	 * - EC_MKBP_FP_ERR_MATCH_YES_UPDATED for match when
	 * template was updated
	 * - EC_MKBP_FP_ERR_MATCH_YES_UPDATE_FAILED match,
	 * but update failed (not saved)
	 * - EC_MKBP_FP_ERR_MATCH_LOW_QUALITY when matching
	 * could not be performed due to low image quality
	 * - EC_MKBP_FP_ERR_MATCH_LOW_COVERAGE when matching
	 * could not be performed due to finger covering too
	 * little area of the sensor
	 */
	int (*finger_match)(void *templ, uint32_t templ_count, uint8_t *image,
			    int32_t *match_index, uint32_t *update_bitmap);

	/*
	 * Start a finger enrollment session.
	 *
	 * @return 0 on success or a negative error code.
	 */
	int (*enrollment_begin)(void);

	/*
	 * Generate a template from the finger whose
	 * enrollment has just being completed.
	 *
	 * @param templ the buffer which will receive the
	 * template. templ can be set to NULL to abort the
	 * current enrollment process.
	 *
	 * @return 0 on success or a negative error code.
	 */
	int (*enrollment_finish)(void *templ);

	/**
	 * Runs a test for defective pixels.
	 *
	 * Should be triggered periodically by the client.
	 * The maintenance command can take several hundred
	 * milliseconds to run.
	 *
	 * @return EC_ERROR_HW_INTERNAL on error (such as
	 * finger on sensor)
	 * @return EC_SUCCESS on success
	 */
	int (*maintenance)(void);

	/* Size of one unencrypted fingerprint template. */
	int algorithm_template_size;

	/* Size of one encrypted fingerprint template sent
	 * to the AP.
	 */
	int encrypted_template_size;

	/* Sensor resolution. */
	int res_x;
	int res_y;
};

extern struct fp_sensor_interface *fp_driver;

/*
 * Image captured but quality is too low
 */
#define FP_SENSOR_LOW_IMAGE_QUALITY 1
/**
 * Finger removed before image was captured
 */
#define FP_SENSOR_TOO_FAST 2

/**
 * Sensor not fully covered by finger
 */
#define FP_SENSOR_LOW_SENSOR_COVERAGE 3

/**
 * Acquires a fingerprint image.
 *
 * This function is called once the finger has been detected and cover enough
 * area of the sensor (i.e., fp_sensor_finger_status returned FINGER_PRESENT).
 * It does the acquisition immediately.
 *
 * @param[out] image_data Image from sensor. Buffer must be allocated by
 * caller with size FP_SENSOR_IMAGE_SIZE.
 *
 * @return 0 on success
 * @return negative value on error
 * @return FP_SENSOR_LOW_IMAGE_QUALITY on image captured but quality is too low
 * @return FP_SENSOR_TOO_FAST on finger removed before image was captured
 * @return FP_SENSOR_LOW_SENSOR_COVERAGE on sensor not fully covered by finger
 */
int fp_sensor_acquire_image(uint8_t *image_data);

/**
 * Acquires a fingerprint image with specific capture mode.
 *
 * Same as the fp_sensor_acquire_image function(),
 * except @p mode can be set to one of the fp_capture_type constants
 * to get a specific image type (e.g. a pattern) rather than the default one.
 *
 * @param[out] image_data Image from sensor. Buffer must be allocated by
 * caller with size FP_SENSOR_IMAGE_SIZE.
 * @param mode  enum fp_capture_type
 *
 * @return 0 on success
 * @return negative value on error
 */
int fp_sensor_acquire_image_with_mode(uint8_t *image_data, int mode);

/**
 * Test that size+offset does not exceed buffer_size
 *
 * Returns:
 *   EC_ERROR_OVERFLOW: if size+offset does not fit in uint32_t
 *   EC_ERROR_INVAL: if size+offset > buffer_size
 *   EC_SUCCESS: otherwise
 */
int fp_sensor_validate_buffer_offset(uint32_t buffer_size, uint32_t offset,
				     uint32_t size);

/**
 * Runs a test for defective pixels.
 *
 * Should be triggered periodically by the client. The maintenance command can
 * take several hundred milliseconds to run.
 *
 * @return EC_ERROR_HW_INTERNAL on error (such as finger on sensor)
 * @return EC_SUCCESS on success
 */
int fp_maintenance(void);
int fp_sensor_validate_buffer_offset(uint32_t buffer_size, uint32_t offset,
				     uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_FPSENSOR_H */
