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

/* Number of dead pixels detected on the last maintenance */
#define FINGERPRINT_ERROR_DEAD_PIXELS(errors) ((errors)&0x3FF)
/* Unknown number of dead pixels detected on the last maintenance */
#define FINGERPRINT_ERROR_DEAD_PIXELS_UNKNOWN (0x3FF)
/* No interrupt from the sensor */
#define FINGERPRINT_ERROR_NO_IRQ BIT(12)
/* SPI communication error */
#define FINGERPRINT_ERROR_SPI_COMM BIT(13)
/* Invalid sensor Hardware ID */
#define FINGERPRINT_ERROR_BAD_HWID BIT(14)
/* Sensor initialization failed */
#define FINGERPRINT_ERROR_INIT_FAIL BIT(15)

/**
 * @brief Get fingerprint sensor width.
 */
#define FINGERPRINT_SENSOR_RES_X(node_id) DT_PROP(node_id, width)

/**
 * @brief Get fingerprint sensor height.
 */
#define FINGERPRINT_SENSOR_RES_Y(node_id) DT_PROP(node_id, height)

/**
 * @brief Get fingerprint sensor width.
 */
#define FINGERPRINT_SENSOR_RES_BPP(node_id) DT_PROP(node_id, bits_per_pixel)

/**
 * @brief Get fingerprint sensor width.
 */
#define FINGERPRINT_SENSOR_V4L2_PIXEL_FORMAT(node_id) \
	DT_PROP(node_id, v4l2_pixel_format)

/**
 * @brief Get size of raw fingerprint image (in bytes)
 */
#define FINGERPRINT_SENSOR_REAL_IMAGE_SIZE(node_id) \
	((FINGERPRINT_SENSOR_RES_X(node_id) *       \
	  FINGERPRINT_SENSOR_RES_Y(node_id) *       \
	  FINGERPRINT_SENSOR_RES_BPP(node_id)) /    \
	 8)

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
 * @brief enum fp_capture_type - Specifies the "mode" when capturing images.
 *
 * @FINGERPRINT_CAPTURE_VENDOR_FORMAT: Capture 1-3 images and choose the best
 * quality image (produces 'frame_size' bytes)
 * @FINGERPRINT_CAPTURE_SIMPLE_IMAGE: Simple raw image capture (produces width x
 * height x bpp bits)
 * @FINGERPRINT_CAPTURE_PATTERN0: Self test pattern (e.g. checkerboard)
 * @FINGERPRINT_CAPTURE_PATTERN1: Self test pattern (e.g. inverted checkerboard)
 * @FINGERPRINT_CAPTURE_QUALITY_TEST: Capture for Quality test with fixed
 * contrast
 * @FINGERPRINT_CAPTURE_RESET_TEST: Capture for pixel reset value test
 * @FINGERPRINT_CAPTURE_TYPE_MAX: End of enum
 *
 * @note This enum must remain ordered, if you add new values you must ensure
 * that FINGERPRINT_CAPTURE_TYPE_MAX is still the last one.
 */
enum fingerprint_capture_type {
	FINGERPRINT_CAPTURE_VENDOR_FORMAT = 0,
	FINGERPRINT_CAPTURE_SIMPLE_IMAGE = 1,
	FINGERPRINT_CAPTURE_PATTERN0 = 2,
	FINGERPRINT_CAPTURE_PATTERN1 = 3,
	FINGERPRINT_CAPTURE_QUALITY_TEST = 4,
	FINGERPRINT_CAPTURE_RESET_TEST = 5,
	FINGERPRINT_CAPTURE_TYPE_MAX,
};

enum fingerprint_finger_state {
	FINGER_STATE_NONE = 0,
	FINGER_STATE_PARTIAL = 1,
	FINGER_STATE_PRESENT = 2,
};

/**
 * @brief Fingerprint callback for fingerprint events
 *
 * @param dev Fingerprint sensor device
 * @param event Call reason
 */
typedef void (*fingerprint_callback_t)(const struct device *dev,
				       uint32_t event);

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
typedef int (*fingerprint_config_api)(const struct device *dev,
				      fingerprint_callback_t cb);

/**
 * @typedef fingerprint_get_info_api
 * @brief Callback API for getting information about fingerprint sensor.
 * @param dev Fingerprint sensor device.
 * @param info Pointer to fingerprint_info structure where data will be stored.
 */
typedef int (*fingerprint_get_info_api)(const struct device *dev,
					struct fingerprint_info *info);

/**
 * @typedef fingerprint_maintenance_api
 * @brief Callback API for maintenance operation.
 * @param dev Fingerprint sensor device.
 */
typedef int (*fingerprint_maintenance_api)(const struct device *dev,
					   uint8_t *buf, size_t size);

/**
 * @typedef fingerprint_enter_low_power_api
 * @brief Callback API for entering low power mode.
 * @param dev Fingerprint sensor device.
 */
typedef int (*fingerprint_enter_low_power_api)(const struct device *dev);

/**
 * @typedef fingerprint_configure_detect
 * @brief Callback API for configuring finger detection.
 * @param dev Fingerprint sensor device.
 */
typedef int (*fingerprint_configure_detect_api)(const struct device *dev);

/**
 * Image captured but quality is too low
 */
#define FINGERPRINT_SENSOR_LOW_IMAGE_QUALITY 1
/**
 * Finger removed before image was captured
 */
#define FINGERPRINT_SENSOR_TOO_FAST 2

/**
 * Sensor not fully covered by finger
 */
#define FINGERPRINT_SENSOR_LOW_SENSOR_COVERAGE 3

/**
 * @typedef fingerprint_acquire_image
 * @brief Callback API for acquiring fingerprint image.
 * @param dev Fingerprint sensor device.
 * @param mode One of the mode from fingerprint_capture_type enum.
 * @param status Pointer to variable where image status should be written
 * @param image Pointer to buffer where image should be stored.
 * @param size Size of the buffer.
 */
typedef int (*fingerprint_acquire_image_api)(const struct device *dev, int mode,
					     uint32_t *status, uint8_t *image,
					     size_t size);

/**
 * typedef fingerprint_finger_status
 * @brief Callback API for the status of the finger on the sensor
 * @param dev Fingerprint sensor device.
 * @param status Pointer to variable where status should be written
 */
typedef int (*fingerprint_finger_status_api)(const struct device *dev,
					     uint32_t *status);

/** @brief Driver API structure. */
__subsystem struct fingerprint_driver_api {
	fingerprint_init_api init;
	fingerprint_deinit_api deinit;
	fingerprint_config_api config;
	fingerprint_get_info_api get_info;
	fingerprint_maintenance_api maintenance;
	fingerprint_enter_low_power_api enter_low_power;
	fingerprint_configure_detect_api configure_detect;
	fingerprint_acquire_image_api acquire_image;
	fingerprint_finger_status_api finger_status;
};

/**
 * @brief Initialize fingerprint sensor.
 *
 * @param dev Pointer to the device structure for the fingerprint sensor driver
 *            instance.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int fingerprint_init(const struct device *dev);

static inline int z_impl_fingerprint_init(const struct device *dev)
{
	const struct fingerprint_driver_api *api =
		(const struct fingerprint_driver_api *)dev->api;

	if (!api->init) {
		return -ENOTSUP;
	}

	return api->init(dev);
}

/**
 * @brief Deinitialize fingerprint sensor.
 *
 * @param dev Pointer to the device structure for the fingerprint sensor driver
 *            instance.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int fingerprint_deinit(const struct device *dev);

static inline int z_impl_fingerprint_deinit(const struct device *dev)
{
	const struct fingerprint_driver_api *api =
		(const struct fingerprint_driver_api *)dev->api;

	if (!api->deinit) {
		return -ENOTSUP;
	}

	return api->deinit(dev);
}

/**
 * @brief Configure fingerprint sensor.
 *
 * @param dev Pointer to the device structure for the fingerprint sensor driver
 *            instance.
 * @param cb  Callback executed on fingerprint event.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int fingerprint_config(const struct device *dev,
				 fingerprint_callback_t cb);

static inline int z_impl_fingerprint_config(const struct device *dev,
					    fingerprint_callback_t cb)
{
	const struct fingerprint_driver_api *api =
		(const struct fingerprint_driver_api *)dev->api;

	if (!api->config) {
		return -ENOTSUP;
	}

	return api->config(dev, cb);
}

/**
 * @brief Get information about fingerprint sensor.
 *
 * @param dev  Pointer to the device structure for the fingerprint sensor driver
 *             instance.
 * @param info Pointer to 'fingerprint_info' structure where data will be
 *             stored.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int fingerprint_get_info(const struct device *dev,
				   struct fingerprint_info *info);

static inline int z_impl_fingerprint_get_info(const struct device *dev,
					      struct fingerprint_info *info)
{
	const struct fingerprint_driver_api *api =
		(const struct fingerprint_driver_api *)dev->api;

	if (!api->get_info) {
		return -ENOTSUP;
	}

	return api->get_info(dev, info);
}

/**
 * @brief Start finegrprint maintenance operation.
 *
 * @param dev Pointer to the device structure for the fingerprint sensor driver
 *            instance.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int fingerprint_maintenance(const struct device *dev, uint8_t *buf,
				      size_t size);

static inline int z_impl_fingerprint_maintenance(const struct device *dev,
						 uint8_t *buf, size_t size)
{
	const struct fingerprint_driver_api *api =
		(const struct fingerprint_driver_api *)dev->api;

	if (!api->maintenance) {
		return -ENOTSUP;
	}

	return api->maintenance(dev, buf, size);
}

/**
 * @brief Enter low power mode.
 *
 * @param dev Pointer to the device structure for the fingerprint sensor driver
 *            instance.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int fingerprint_enter_low_power(const struct device *dev);

static inline int z_impl_fingerprint_enter_low_power(const struct device *dev)
{
	const struct fingerprint_driver_api *api =
		(const struct fingerprint_driver_api *)dev->api;

	if (!api->enter_low_power) {
		return -ENOTSUP;
	}

	return api->enter_low_power(dev);
}

/**
 * @brief Configure sensor to detect the presence of a finger..
 *
 * @param dev Pointer to the device structure for the fingerprint sensor driver
 *            instance.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int fingerprint_configure_detect(const struct device *dev);

static inline int z_impl_fingerprint_configure_detect(const struct device *dev)
{
	const struct fingerprint_driver_api *api =
		(const struct fingerprint_driver_api *)dev->api;

	if (!api->configure_detect) {
		return -ENOTSUP;
	}

	return api->configure_detect(dev);
}

/**
 * @brief Acquire image of a finger.
 *
 * @param dev Pointer to the device structure for the fingerprint sensor driver
 *            instance.
 * @param mode One of the mode from fingerprint_capture_type enum.
 * @param status Pointer to variable where image status should be written
 * @param image Pointer to buffer where image should be stored.
 * @param size Size of the buffer.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int fingerprint_acquire_image(const struct device *dev, int mode,
					uint32_t *status, uint8_t *image,
					size_t size);

static inline int z_impl_fingerprint_acquire_image(const struct device *dev,
						   int mode, uint32_t *status,
						   uint8_t *image, size_t size)
{
	const struct fingerprint_driver_api *api =
		(const struct fingerprint_driver_api *)dev->api;

	if (!api->acquire_image) {
		return -ENOTSUP;
	}

	return api->acquire_image(dev, mode, status, image, size);
}

/**
 * @brief Get status of the finger on the sensor.
 *
 * @param dev Pointer to the device structure for the fingerprint sensor driver
 *            instance.
 * @param status Pointer to variable where status should be written
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int fingerprint_finger_status(const struct device *dev,
					uint32_t *status);

static inline int z_impl_fingerprint_finger_status(const struct device *dev,
						   uint32_t *status)
{
	const struct fingerprint_driver_api *api =
		(const struct fingerprint_driver_api *)dev->api;

	if (!api->finger_status) {
		return -ENOTSUP;
	}

	return api->finger_status(dev, status);
}

/**
 * @}
 */
#include <syscalls/cros_fingerprint.h>
#endif /* ZEPHYR_INCLUDE_DRIVERS_FINGERPRINT_H_ */
