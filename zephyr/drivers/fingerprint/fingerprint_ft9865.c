/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT ft_ft9865

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

#include <drivers/fingerprint.h>
#include <fingerprint/v4l2_types.h>

LOG_MODULE_REGISTER(cros_fingerprint, LOG_LEVEL_INF);

// TODO implement the driver

static int ft9865_set_mode(const struct device *dev,
			    enum fingerprint_sensor_mode mode)
{
	return 0;
}

static int ft9865_init(const struct device *dev)
{
	return 0;
}

static int ft9865_deinit(const struct device *dev)
{
	return 0;
}

static int ft9865_get_info(const struct device *dev,
			    struct fingerprint_info *info)
{
	info->model_id = 0x9865;
	info->vendor_id = 0xff00;
	info->errors = 0;

	return 0;
}

static int ft9865_config(const struct device *dev, fingerprint_callback_t cb)
{
	return 0;
}

static int ft9865_maintenance(const struct device *dev, uint8_t *buf,
			       size_t size)
{
	return 0;
}

static int ft9865_acquire_image(const struct device *dev,
				 enum fingerprint_capture_type capture_type,
				 uint8_t *image_buf, size_t image_buf_size)
{
	return 0;
}

static int ft9865_finger_status(const struct device *dev)
{
	return 0;
}

static DEVICE_API(fingerprint, cros_fp_ft9865_driver_api) = {
	.init = ft9865_init,
	.deinit = ft9865_deinit,
	.config = ft9865_config,
	.get_info = ft9865_get_info,
	.maintenance = ft9865_maintenance,
	.set_mode = ft9865_set_mode,
	.acquire_image = ft9865_acquire_image,
	.finger_status = ft9865_finger_status,
};

static int ft9865_init_driver(const struct device *dev)
{
	return 0;
}

#define FT9865_DEFINE(inst)                                                   \
	DEVICE_DT_INST_DEFINE(inst, ft9865_init_driver, NULL,                 \
			      NULL, NULL,       \
			      POST_KERNEL,                                     \
			      CONFIG_FINGERPRINT_SENSOR_INIT_PRIORITY,         \
			      &cros_fp_ft9865_driver_api)

DT_INST_FOREACH_STATUS_OKAY(FT9865_DEFINE);
