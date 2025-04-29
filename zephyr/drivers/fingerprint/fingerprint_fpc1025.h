/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_DRIVERS_FINGERPRINT_FPC1025_H_
#define ZEPHYR_DRIVERS_FINGERPRINT_FPC1025_H_

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

#include <drivers/fingerprint.h>

struct fp_image_frame_params {
	/* Image frame characteristics */
	uint32_t frame_size;
	uint32_t pixel_format; /* using V4L2_PIX_FMT_ */
	uint16_t width;
	uint16_t height;
	uint16_t bpp;
	/** Type of image capture from enum fp_capture_type. */
	uint8_t fp_capture_type;
	uint8_t reserved; /**< padding for alignment */
};

struct fpc1025_cfg {
	struct spi_dt_spec spi;
	struct gpio_dt_spec interrupt;
	struct gpio_dt_spec reset_pin;
	struct fingerprint_sensor_info sensor_info;
	struct fp_image_frame_params sensor_image_configs[];
};

struct fpc1025_data {
	const struct device *dev;
	fingerprint_callback_t callback;
	struct gpio_callback irq_cb;
	struct k_sem sensor_lock;
	k_tid_t sensor_owner;
	uint16_t errors;
};

#endif /* ZEPHYR_DRIVERS_FINGERPRINT_FPC1025_H_ */
