/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief API for fingerprint sensors
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_FINGERPRINT_H_
#define ZEPHYR_INCLUDE_DRIVERS_FINGERPRINT_H_

/**
 * @brief Fingerprint sensor Interface
 * @defgroup fingerprint_interface fingerprint Interface
 * @ingroup io_interfaces
 * @{
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>

/**
 * @brief Fingerprint sensor information structure
 */
struct fingerprint_info {
	/* Sensor identification */
	uint32_t vendor_id;
	uint32_t product_id;
	uint32_t model_id;
	uint32_t version;
	/* Image frame characteristics */
	uint32_t frame_size;
	uint32_t pixel_format; /* using V4L2_PIX_FMT_ */
	uint16_t width;
	uint16_t height;
	uint16_t bpp;
	uint16_t errors;
};

/**
 * @brief Fingerprint callback for fingerprint events
 *
 * @param dev Fingerprint sensor device
 * @param event Call reason
 */
typedef void (*fingerprint_callback_t)(const struct device *dev, uint32_t event)

/**
 * @typedef fingerprint_init_api
 * @param dev Fingerprint sensor device.
 * @brief Callback API for initializing fingerprint sensor.
 */
typedef int (*fingerprint_init_api)(const struct device *dev);

/**
 * @typedef fingerprint_deinit_api
 * @param dev Fingerprint sensor device.
 * @brief Callback API for deinitializing fingerprint sensor.
 */
typedef int (*fingerprint_deinit_api)(const struct device *dev);

/**
 * @typedef fingerprint_config_api
 * @param dev Fingerprint sensor device.
 * @param cb Callback executed on event.
 * @brief Callback API for configuring fingerprint sensor.
 */
typedef int (*fingerprint_config_api)(const struct device *dev, fingerprint_callback_t cb);

/**
 * @typedef fingerprint_get_info_api
 * @brief Callback API for getting information about fingerprint sensor.
 * @param dev Fingerprint sensor device.
 * @param info Pointer to fingerprint_info structure where data will be stored.
 */
typedef int (*fingerprint_get_info_api)(const struct device *dev, struct fingerprint_info *info);

/**
 * @typedef fingerprint_enroll_start_api
 * @brief Callback API for starting enrollment.
 * @param dev Fingerprint sensor device.
 */
typedef int (*fingerprint_enroll_start_api)(const struct device *dev);

/**
 * @typedef fingerprint_enroll_step_api
 * @brief Callback API for one enrollment step.
 * @param dev Fingerprint sensor device.
 */
typedef int (*fingerprint_enroll_step_api)(const struct device *dev);

/**
 * @typedef fingerprint_enroll_finish_api
 * @brief Callback API for finishing enrollment.
 * @param dev Fingerprint sensor device.
 */
typedef int (*fingerprint_enroll_finish_api)(const struct device *dev);

/**
 * @typedef fingerprint_match_api
 * @brief Callback API for match operation.
 * @param dev Fingerprint sensor device.
 */
typedef int (*fingerprint_match_api)(const struct device *dev);

/**
 * @typedef fingerprint_maintenance_api
 * @brief Callback API for maintenance operation.
 * @param dev Fingerprint sensor device.
 */
typedef int (*fingerprint_maintenance_api)(const struct device *dev);

/**
 * @typedef fingerprint_enter_low_power_api
 * @brief Callback API for entering low power mode.
 * @param dev Fingerprint sensor device.
 */
typedef int (*fingerprint_enter_low_power_api)(const struct device *dev);


/** @brief Driver API structure. */
__subsystem struct fingerprint_driver_api {
	fingerprint_init_api init;
	fingerprint_deinit_api deinit;
	fingerprint_config_api config;
	fingerprint_get_info_api get_info;
	fingerprint_enroll_start_api enroll_start;
	fingerprint_enroll_step_api enroll_step;
	fingerprint_enroll_finish_api enroll_finish;
	fingerprint_match_api match;
	fingerprint_maintenance_api maintenance;
	fignerprint_enter_low_power_api enter_low_power;
};


/**
 * @}
 */
#include <syscalls/fingerprint.h>
#endif /* ZEPHYR_INCLUDE_DRIVERS_FINGERPRINT_H_ */
