// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cerrno>
#define DT_DRV_COMPAT egis_egis630

#include "fingerprint_egis630.h"
#include "fingerprint_egis630_private.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/linker/section_tags.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

#include <drivers/fingerprint.h>

LOG_MODULE_REGISTER(cros_fingerprint, LOG_LEVEL_INF);

static inline int egis630_enable_irq(const struct device *dev)
{
	const struct egis630_cfg *cfg = dev->config;
	int rc;

	rc = gpio_pin_interrupt_configure_dt(&cfg->interrupt,
					     GPIO_INT_EDGE_TO_ACTIVE);
	if (rc < 0) {
		LOG_ERR("Can't enable interrupt: %d", rc);
	}

	return rc;
}

static inline int egis630_disable_irq(const struct device *dev)
{
	const struct egis630_cfg *cfg = dev->config;
	int rc;

	rc = gpio_pin_interrupt_configure_dt(&cfg->interrupt, GPIO_INT_DISABLE);
	if (rc < 0) {
		LOG_ERR("Can't disable interrupt: %d", rc);
	}

	return rc;
}

static int convert_egis_get_image_error_code(egis_api_return_t code)
{
	switch (code) {
	case EGIS_API_IMAGE_QUALITY_GOOD:
		return EC_SUCCESS;
	case EGIS_API_IMAGE_QUALITY_BAD:
	case EGIS_API_IMAGE_QUALITY_WATER:
		return FP_SENSOR_LOW_IMAGE_QUALITY;
	case EGIS_API_IMAGE_EMPTY:
		return FP_SENSOR_TOO_FAST;
	case EGIS_API_IMAGE_QUALITY_PARTIAL:
		return FP_SENSOR_LOW_SENSOR_COVERAGE;
	default:
		assert(code < 0);
		return -EINVAL;
	}
}

static int egis630_init(const struct device *dev)
{
	struct egis630_data *data = dev->data;

	data->errors = FINGERPRINT_ERROR_DEAD_PIXELS_UNKNOWN;

	if (IS_ENABLED(CONFIG_HAVE_EGIS630_PRIVATE_DRIVER)) {
		/* Print the binary libfpsensor.a library version. */
		LOG_PRINTK("EGIS libfpsensor.a v%s\n",
			   "ask Egis to get get version");
	}

	egis_fp_reset_sensor();

	int int_pin_value = gpio_pin_get_dt(&cfg->interrupt);

	egis_api_return_t ret = egis_sensor_init();
	if (ret == EGIS_API_ERROR_IO_SPI) {
		data->errors |= FINGERPRINT_ERROR_SPI_COMM;
	} else if (ret == EGIS_API_ERROR_DEVICE_NOT_FOUND) {
		data->errors |= FINGERPRINT_ERROR_BAD_HWID;
	} else if (ret != EGIS_API_OK) {
		data->errors |= FINGERPRINT_ERROR_INIT_FAIL;
	}

	if (int_pin_value == gpio_pin_get_dt(&cfg->interrupt)) {
		LOG_ERR("Sensor IRQ not ready");
		data->errors |= FINGERPRINT_ERROR_NO_IRQ;
	}

	return 0;
}

static int egis630_deinit(const struct device *dev)
{
	if (!IS_ENABLED(CONFIG_HAVE_FPC1025_PRIVATE_DRIVER)) {
		return 0;
	}

	egis_api_return_t ret = egis_sensor_deinit();
	if (ret < 0) {
		LOG_ERR("egis_sensor_deinit() failed, result %d", ret);
		return ret;
	}

	return 0;
}

static int egis630_config(const struct device *dev, fingerprint_callback_t cb)
{
	struct egis630_data *data = dev->data;

	data->callback = cb;

	return 0;
}

static int egis630_get_info(const struct device *dev,
			    struct fingerprint_info *info)
{
	const struct egis630_cfg *cfg = dev->config;
	struct egis630_data *data = dev->data;
	uint16_t sensor_id;

	memcpy(info, &cfg->info, sizeof(struct fingerprint_info));

	egis_api_return_t res = egis_get_hwid(&sensor_id);
	if (res != EGIS_API_OK) {
		LOG_ERR("Failed to get EGIS HWID: %d", rc);
		return ret;
	}

	info->model_id = sensor_id;
	info->errors = errors;

	return 0;
}

static int egis630_maintenance(const struct device *dev, uint8_t *buf,
			       size_t size)
{
	if (!IS_ENABLED(CONFIG_HAVE_EGIS630_PRIVATE_DRIVER)) {
		return -ENOTSUP;
	}

	if (size < CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE)
		return -EINVAL;

	return 0;
}

static int egis630_set_mode(const struct device *dev,
			    enum fingerprint_sensor_mode mode)
{
	int rc = 0;

	switch (mode) {
	case FINGERPRINT_SENSOR_MODE_DETECT:
		if (IS_ENABLED(CONFIG_HAVE_EGIS630_PRIVATE_DRIVER)) {
			LOG_INF("");
			egis_set_detect_mode();
			rc = egis630_enable_irq(dev);
		} else {
			rc = -ENOTSUP;
		}
		break;

	case FINGERPRINT_SENSOR_MODE_LOW_POWER:
		if (IS_ENABLED(CONFIG_HAVE_EGIS630_PRIVATE_DRIVER)) {
			rc = egis_sensor_power_down();
		} else {
			rc = -ENOTSUP;
		}
		break;

	case FINGERPRINT_SENSOR_MODE_IDLE:
		rc = egis630_disable_irq(dev);
		break;

	default:
		rc = -ENOTSUP;
	}

	return rc;
}

static int egis630_acquire_image(const struct device *dev, int mode,
				 uint8_t *image_buf, size_t image_buf_size)
{
	if (image_buf_size < CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE)
		return -EINVAL;

	if (!IS_ENABLED(CONFIG_HAVE_EGIS630_PRIVATE_DRIVER)) {
		return -ENOTSUP;
	}

	return convert_egis_get_image_error_code(
		egis_get_image_with_mode(image_data, mode));
}

static int egis630_finger_status(const struct device *dev)
{
	if (!IS_ENABLED(CONFIG_HAVE_EGIS630_PRIVATE_DRIVER)) {
		return -ENOTSUP;
	}

	LOG_INF("");
	egis_api_return_t ret = egis_check_int_status();

	switch (ret) {
	case EGIS_API_FINGER_PRESENT:
		return FINGERPRINT_FINGER_STATE_PRESENT;
	case EGIS_API_FINGER_LOST:
	default:
		return FINGERPRINT_FINGER_STATE_NONE;
	}
}

static const struct fingerprint_driver_api cros_fp_egis630_driver_api = {
	.init = egis630_init,
	.deinit = egis630_deinit,
	.config = egis630_config,
	.get_info = egis630_get_info,
	.maintenance = egis630_maintenance,
	.set_mode = egis630_set_mode,
	.acquire_image = egis630_acquire_image,
	.finger_status = egis630_finger_status,
};
