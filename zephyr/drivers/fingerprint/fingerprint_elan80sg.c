/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <cerrno>
#define DT_DRV_COMPAT elan_elan80sg

#include "fingerprint_elan80sg.h"
#include "fingerprint_elan80sg_private.h"

#include <assert.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

#include <drivers/fingerprint.h>
#include <fingerprint/v4l2_types.h>

LOG_MODULE_REGISTER(cros_fingerprint, LOG_LEVEL_INF);

static enum elan_capture_type
convert_fp_capture_mode_to_elan_get_image_type(enum fp_capture_type mode)
{
	switch (mode) {
	case FINGERPRINT_CAPTURE_TYPE_VENDOR_FORMAT:
		return ELAN_CAPTURE_VENDOR_FORMAT;
	case FINGERPRINT_CAPTURE_TYPE_SIMPLE_IMAGE:
		return ELAN_CAPTURE_SIMPLE_IMAGE;
	case FINGERPRINT_CAPTURE_TYPE_PATTERN0:
		return ELAN_CAPTURE_PATTERN0;
	case FINGERPRINT_CAPTURE_TYPE_PATTERN1:
		return ELAN_CAPTURE_PATTERN1;
	case FINGERPRINT_CAPTURE_TYPE_QUALITY_TEST:
		return ELAN_CAPTURE_QUALITY_TEST;
	case FINGERPRINT_CAPTURE_TYPE_RESET_TEST:
		return ELAN_CAPTURE_RESET_TEST;
	default:
		return ELAN_CAPTURE_TYPE_INVALID;
	}
}

void fp_sensor_lock(const struct device *dev)
{
	__maybe_unused const struct elan80sg_cfg *cfg = dev->config;
	struct elan80sg_data *data = dev->data;

	/* Lock SPI access only if we are not already the owner. */
	if (!((k_sem_count_get(&data->sensor_lock) == 0) &&
	      (data->sensor_owner == k_current_get()))) {
		k_sem_take(&data->sensor_lock, K_FOREVER);
		data->sensor_owner = k_current_get();

#ifdef CONFIG_PM_DEVICE
		/* Enable clock gating for SPI module and configure SPI pins
		 * into an alternate mode.
		 */
		pm_device_action_run(cfg->spi.bus, PM_DEVICE_ACTION_RESUME);
#endif /* CONFIG_PM_DEVICE */
	}
}

void fp_sensor_unlock(const struct device *dev)
{
	__maybe_unused const struct elan80sg_cfg *cfg = dev->config;
	struct elan80sg_data *data = dev->data;

#ifdef CONFIG_PM_DEVICE
	/* Disable SPI mainly to reconfigure SPI pins to sleep state
	 * (CLK, MISO, MOSI set to output low) to reduce power
	 * consumption by the sensor.
	 *
	 * The SPI drivers disable the SPI module after a transaction,
	 * which puts the pins into floating state.
	 */
	pm_device_action_run(cfg->spi.bus, PM_DEVICE_ACTION_SUSPEND);
#endif /* CONFIG_PM_DEVICE */

	/* Clear the owner and return the access. */
	data->sensor_owner = NULL;
	k_sem_give(&data->sensor_lock);
}

static int elan80sg_get_hwid(const struct device *dev, uint16_t *id)
{
	int rc;
	uint8_t id_hi = 0, id_lo = 0;

	if (id == NULL)
		return -EINVAL;

	rc = elan_read_register(0x02, &id_hi);
	rc |= elan_read_register(0x04, &id_lo);
	if (rc) {
		LOG_INF("ELAN HW ID read failed %d", rc);
		return -ENOTSUP;
	}
	*id = (id_hi << 8) | id_lo;

	return 0;
}

static int elan80sg_check_hwid(const struct device *dev, uint16_t *id)
{
	const struct elan80sg_cfg *cfg = dev->config;
	struct elan80sg_data *data = dev->data;

	uint16_t id = 0;
	int status;

	status = elan80sg_get_hwid(dev, &id);
	if (status < 0) {
		assert(status == -ENOTSUP);
		data->errors |= FINGERPRINT_ERROR_SPI_COMM;
	}

	if (id != FP_SENSOR_HWID_ELAN) {
		LOG_INF("ELAN unknown silicon 0x%04x", id);
		data->errors |= FINGERPRINT_ERROR_BAD_HWID;
		return -ENOTSUP;
	}

	LOG_INF("ELAN HWID 0x%04x", id);
	return 0;
}

static inline int elan80sg_enable_irq(const struct device *dev)
{
	const struct elan80sg_cfg *cfg = dev->config;
	int rc;

	rc = gpio_pin_interrupt_configure_dt(&cfg->interrupt,
					     GPIO_INT_EDGE_TO_ACTIVE);
	if (rc < 0) {
		LOG_ERR("Can't enable interrupt: %d", rc);
	}

	return rc;
}

static inline int elan80sg_disable_irq(const struct device *dev)
{
	const struct elan80sg_cfg *cfg = dev->config;
	int rc;

	rc = gpio_pin_interrupt_configure_dt(&cfg->interrupt, GPIO_INT_DISABLE);
	if (rc < 0) {
		LOG_ERR("Can't disable interrupt: %d", rc);
	}

	return rc;
}

static int elan80sg_set_mode(const struct device *dev,
			     enum fingerprint_sensor_mode mode)
{
	LOG_INF("========%s=======\n", __func__);

	int rc = 0;

	switch (mode) {
	case FINGERPRINT_SENSOR_MODE_DETECT:
		if (IS_ENABLED(CONFIG_HAVE_ELAN80SG_PRIVATE_DRIVER)) {
			rc = elan_woe_mode();
			if (rc == 0) {
				rc = elan80sg_enable_irq(dev);
			}
		} else {
			rc = -ENOTSUP;
		}
		break;

	case FINGERPRINT_SENSOR_MODE_LOW_POWER:
		if (IS_ENABLED(CONFIG_HAVE_ELAN80SG_PRIVATE_DRIVER)) {
			rc = elan80sg_disable_irq(dev);
			if (rc == 0) {
				rc = elan_woe_mode();
			}
		} else {
			rc = -ENOTSUP;
		}
		break;

	case FINGERPRINT_SENSOR_MODE_IDLE:
		rc = elan80sg_disable_irq(dev);
		break;

	default:
		rc = -ENOTSUP;
	}

	return rc;
}

static int elan80sg_init(const struct device *dev)
{
	LOG_INF("========%s=======\n", __func__);

	const struct elan80sg_cfg *cfg = dev->config;
	struct elan80sg_data *data = dev->data;
	int rc;

	data->errors = FINGERPRINT_ERROR_DEAD_PIXELS_UNKNOWN;

	elan_execute_reset();
	if (IS_ENABLED(CONFIG_HAVE_ELAN80SG_PRIVATE_DRIVER)) {
		rc = elan_alg_param_setting();
		if (rc != 0) {
			return rc;
		}
	} else {
		return -EINVAL;
	}
	elan_set_hv_chip(1);

	int rc = elan_check_hwid();
	if (rc != 0) {
		data->errors |= FINGERPRINT_ERROR_NO_IRQ;
		return 0;
	}

	if (elan_execute_calibration() < 0)
		data->errors |= FINGERPRINT_ERROR_NO_IRQ;
	if (elan_woe_mode() != 0)
		data->errors |= FINGERPRINT_ERROR_SPI_COMM;

	return 0;
}

static int elan80sg_deinit(const struct device *dev)
{
	LOG_INF("========%s=======\n", __func__);

	if (!IS_ENABLED(CONFIG_HAVE_ELAN80SG_PRIVATE_DRIVER)) {
		return 0;
	}

	int rc = elan_fp_deinit();

	if (rc != 0) {
		LOG_ERR("elan_sensor_deinit() failed, result %d", rc);
		return rc;
	}

	return 0;
}

static int elan80sg_get_info(const struct device *dev,
			     struct fingerprint_info *info)
{
	LOG_INF("========%s=======\n", __func__);

	const struct elan80sg_cfg *cfg = dev->config;
	struct elan80sg_data *data = dev->data;
	uint16_t id = 0;

	memcpy(info, &cfg->info, sizeof(struct fingerprint_info));

	if (elan80sg_get_hwid(dev, &id)) {
		return -EINVAL;
	}

	info->model_id = id;
	info->errors = errors;

	return 0;
}

static int elan80sg_config(const struct device *dev, fingerprint_callback_t cb)
{
	struct elan80sg_data *data = dev->data;

	data->callback = cb;

	return 0;
}

static int elan80sg_maintenance(const struct device *dev, uint8_t *buf,
				size_t size)
{
	LOG_INF("========%s=======\n", __func__);

	if (size < CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE) {
		return -EINVAL;
	}

	if (!IS_ENABLED(CONFIG_HAVE_ELAN80SG_PRIVATE_DRIVER)) {
		return 0;
	}
	struct elan80sg_data *data = dev->data;

	int rc = elan_fp_maintenance(&data->errors);

	if (rc != 0) {
		LOG_ERR("elan_fp_maintenance() failed, result %d", rc);
		return rc;
	}

	return 0;
}

static int elan80sg_acquire_image(const struct device *dev, int mode,
				  uint8_t *image_buf, size_t image_buf_size)
{
	LOG_INF("========%s=======\n", __func__);

	if (image_buf_size < CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE) {
		return -EINVAL;
	}

	if (!IS_ENABLED(CONFIG_HAVE_ELAN80SG_PRIVATE_DRIVER)) {
		return -ENOTSUP;
	}

	enum elan_capture_type rc =
		convert_fp_capture_mode_to_elan_get_image_type(mode);

	if (rc == ELAN_CAPTURE_TYPE_INVALID) {
		LOG_ERR("Unsupported mode %d provided", mode);
		return -EINVAL;
	}

	rc = fp_sensor_acquire_image_with_mode(image_buf, rc);
	if (rc < 0) {
		LOG_ERR("Failed to acquire image with mode %d: %d", mode, rc);
		return rc;
	}

	return rc;
}

static int elan80sg_finger_status(const struct device *dev)
{
	LOG_INF("========%s=======\n", __func__);

	int rc;

	if (!IS_ENABLED(CONFIG_HAVE_ELAN80SG_PRIVATE_DRIVER)) {
		return -ENOTSUP;
	}

	rc = elan_sensor_finger_status();
	if (rc < 0) {
		LOG_ERR("Failed to get finger status: %d", rc);
		return rc;
	}

	return rc;
}

static DEVICE_API(fingerprint, cros_fp_elan80sg_driver_api) = {
	.init = elan80sg_init,
	.deinit = elan80sg_deinit,
	.config = elan80sg_config,
	.get_info = elan80sg_get_info,
	.maintenance = elan80sg_maintenance,
	.set_mode = elan80sg_set_mode,
	.acquire_image = elan80sg_acquire_image,
	.finger_status = elan80sg_finger_status,
};

static void elan80sg_irq(const struct device *dev, struct gpio_callback *cb,
			 uint32_t pins)
{
	struct elan80sg_data *data =
		CONTAINER_OF(cb, struct elan80sg_data, irq_cb);

	elan80sg_disable_irq(data->dev);

	if (data->callback != NULL) {
		data->callback(dev);
	}
}

static int elan80sg_init_driver(const struct device *dev)
{
	const struct elan80sg_cfg *cfg = dev->config;
	struct elan80sg_data *data = dev->data;
	int ret;

	if (!spi_is_ready_dt(&cfg->spi)) {
		LOG_ERR("SPI bus is not ready");
		return -EINVAL;
	}

	if (!gpio_is_ready_dt(&cfg->reset_pin)) {
		LOG_ERR("Port for sensor reset GPIO is not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&cfg->reset_pin, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Can't configure sensor reset pin");
		return ret;
	}

	if (!gpio_is_ready_dt(&cfg->interrupt)) {
		LOG_ERR("Port for interrupt GPIO is not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&cfg->interrupt, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Can't configure interrupt pin");
		return ret;
	}

	k_sem_init(&data->sensor_lock, 1, 1);

	data->dev = dev;
	gpio_init_callback(&data->irq_cb, elan80sg_irq,
			   BIT(cfg->interrupt.pin));
	gpio_add_callback_dt(&cfg->interrupt, &data->irq_cb);

	return 0;
}

#define ELAN80SG_SENSOR_INFO(inst)                                             \
	{                                                                      \
		.vendor_id = FOURCC('E', 'L', 'A', 'N'), .product_id = 0x0903, \
		.model_id = 0x01, .version = 0x100B,                           \
		.frame_size = CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE,            \
		.pixel_format = FINGERPRINT_SENSOR_V4L2_PIXEL_FORMAT(          \
			DT_DRV_INST(inst)),                                    \
		.width = FINGERPRINT_SENSOR_RES_X(DT_DRV_INST(inst)),          \
		.height = FINGERPRINT_SENSOR_RES_Y(DT_DRV_INST(inst)),         \
		.bpp = FINGERPRINT_SENSOR_RES_BPP(DT_DRV_INST(inst)),          \
	}

#define ELAN80SG_DEFINE(inst)                                                  \
	static struct elan80sg_data elan80sg_data_##inst;                      \
	static const struct elan80sg_cfg elan80sg_cfg_##inst = {               \
		.spi = SPI_DT_SPEC_INST_GET(                                   \
			inst,                                                  \
			SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_HOLD_ON_CS, \
			0),                                                    \
		.interrupt = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),           \
		.reset_pin = GPIO_DT_SPEC_INST_GET(inst, reset_gpios),         \
		.info = ELAN80SG_SENSOR_INFO(inst),                            \
	};                                                                     \
	BUILD_ASSERT(                                                          \
		CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE >=                        \
			FINGERPRINT_SENSOR_REAL_IMAGE_SIZE(DT_DRV_INST(inst)), \
		"FP image buffer size is smaller than raw image size");        \
	DEVICE_DT_INST_DEFINE(inst, elan80sg_init_driver, NULL,                \
			      &elan80sg_data_##inst, &elan80sg_cfg_##inst,     \
			      POST_KERNEL,                                     \
			      CONFIG_FINGERPRINT_SENSOR_INIT_PRIORITY,         \
			      &cros_fp_elan80sg_driver_api)

DT_INST_FOREACH_STATUS_OKAY(ELAN80SG_DEFINE);
