/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fingerprint sensor type identifiers */

#ifndef __CROS_EC_FPSENSOR_TYPES_H
#define __CROS_EC_FPSENSOR_TYPES_H

#include <stdint.h>

enum fp_sensor_type {
	FP_SENSOR_TYPE_UNKNOWN = -1,
	FP_SENSOR_TYPE_FPC,
	FP_SENSOR_TYPE_ELAN,
};

enum fp_transport_type {
	FP_TRANSPORT_TYPE_UNKNOWN = -1,
	FP_TRANSPORT_TYPE_SPI,
	FP_TRANSPORT_TYPE_UART
};

enum fp_sensor_spi_select {
	FP_SENSOR_SPI_SELECT_UNKNOWN = -1,
	FP_SENSOR_SPI_SELECT_DEVELOPMENT,
	FP_SENSOR_SPI_SELECT_PRODUCTION
};

enum finger_state {
	FINGER_NONE = 0,
	FINGER_PARTIAL = 1,
	FINGER_PRESENT = 2,
};

struct fpsensor_config {
	/* the 16-bit hardware id is 0x021y */
	uint16_t hwid;

	/* sensor type name */
	char name[16];

	/* sensor pixel resolution */
	uint16_t res_x; /**< sensor width */
	uint16_t res_y; /**< sensor height */
	uint16_t res_bpp; /**< resolution bits per pixel */

	/*
	 * sensor image size
	 *
	 * value from fpc_bep_image_get_buffer_size(): (160*160)+660
	 */
	uint16_t image_size;
	uint16_t real_image_size;

	/* offset of image data in fp_buffer */
	uint16_t image_offset;

	/*
	 * constant value for the enrollment data size
	 *
	 * size of private fp_bio_enrollment_t
	 */
	uint16_t algorithm_enrollment_size;

	/*
	 * constant value corresponding to the maximum template size
	 * for fpc1025 sensor. client template memory allocation must
	 * have this size. this includes extra memory for template update.
	 *
	 * template size + alignment padding + size of template size variable
	 */
	uint16_t algorithm_template_size;

	/* max number of templates stored / matched against */
	uint8_t max_finger_count;
};

#endif /* __CROS_EC_FPSENSOR_TYPES_H */
