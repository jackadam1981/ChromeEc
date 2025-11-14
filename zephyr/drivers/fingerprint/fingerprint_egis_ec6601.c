/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT egis_ec6601

#include "fingerprint_egis_ec6601.h"
#include "fingerprint_egis_ec6601_pal.h"
#include "fingerprint_egis_ec6601_private.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

#include <drivers/fingerprint.h>
#include <fingerprint/v4l2_types.h>

LOG_MODULE_REGISTER(cros_fingerprint, LOG_LEVEL_INF);

/* Provide information about used sensor. */
const struct egis_sensor_info egis_sensor_info = {
	.sensor = &egis_bep_sensor_6601g175,
	.image_buffer_size = CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE,
};

/* Sensor IC commands */
enum ec6601_cmd {
	EC6601_CMD_DEEPSLEEP = 0x2C,
	EC6601_CMD_HW_ID = 0xFC,
};

int convert_fp_capture_type_to_egis_capture_type(
	enum fingerprint_capture_type mode)
{
	switch (mode) {
	case FINGERPRINT_CAPTURE_TYPE_VENDOR_FORMAT:
		return EGIS_CAPTURE_VENDOR_FORMAT;
	case FINGERPRINT_CAPTURE_TYPE_SIMPLE_IMAGE:
		return EGIS_CAPTURE_SIMPLE_IMAGE;
	case FINGERPRINT_CAPTURE_TYPE_PATTERN0:
		return EGIS_CAPTURE_PATTERN0;
	case FINGERPRINT_CAPTURE_TYPE_PATTERN1:
		return EGIS_CAPTURE_PATTERN1;
	case FINGERPRINT_CAPTURE_TYPE_QUALITY_TEST:
		return EGIS_CAPTURE_QUALITY_TEST;
	case FINGERPRINT_CAPTURE_TYPE_RESET_TEST:
		return EGIS_CAPTURE_RESET_TEST;
	default:
		return -EINVAL;
	}
}

/* The 16-bit hardware ID is 0x021y */
#define FP_SENSOR_HWID_EGIS 0x2d1

void fp_sensor_lock(const struct device *dev)
{
	__maybe_unused const struct ec6601_cfg *cfg = dev->config;
	struct ec6601_data *data = dev->data;

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
	__maybe_unused const struct ec6601_cfg *cfg = dev->config;
	struct ec6601_data *data = dev->data;

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

static int ec6601_send_cmd(const struct device *dev, uint8_t cmd)
{
	const struct ec6601_cfg *cfg = dev->config;
	const struct spi_buf tx_buf[] = { { .buf = &cmd, .len = 1 } };
	const struct spi_buf_set tx = { .buffers = tx_buf, .count = 1 };
	int rc;

	fp_sensor_lock(dev);
	rc = spi_write_dt(&cfg->spi, &tx);

	/* Release CS line */
	spi_release_dt(&cfg->spi);
	fp_sensor_unlock(dev);

	return rc;
}

static int ec6601_get_hwid(const struct device *dev, uint16_t *id)
{
	const struct ec6601_cfg *cfg = dev->config;
	uint8_t cmd = EC6601_CMD_HW_ID;
	int rc;

	const struct spi_buf tx_buf[] = { { .buf = &cmd, .len = 1 },
					  { .buf = NULL, .len = 2 } };
	const struct spi_buf rx_buf[] = { { .buf = NULL, .len = 1 },
					  { .buf = id, .len = 2 } };
	const struct spi_buf_set tx = { .buffers = tx_buf,
					.count = ARRAY_SIZE(tx_buf) };
	const struct spi_buf_set rx = { .buffers = rx_buf,
					.count = ARRAY_SIZE(rx_buf) };

	if (id == NULL)
		return -EINVAL;

	fp_sensor_lock(dev);
	rc = spi_transceive_dt(&cfg->spi, &tx, &rx);

	/* Release CS line */
	spi_release_dt(&cfg->spi);
	fp_sensor_unlock(dev);

	/* HWID is in big endian, so convert it CPU endianness. */
	*id = sys_be16_to_cpu(*id);

	return rc;
}

static inline int ec6601_enable_irq(const struct device *dev)
{
	const struct ec6601_cfg *cfg = dev->config;
	int rc;

	rc = gpio_pin_interrupt_configure_dt(&cfg->interrupt,
					     GPIO_INT_EDGE_TO_ACTIVE);
	if (rc < 0) {
		LOG_ERR("Can't enable interrupt: %d", rc);
	}

	return rc;
}

static inline int ec6601_disable_irq(const struct device *dev)
{
	const struct ec6601_cfg *cfg = dev->config;
	int rc;

	rc = gpio_pin_interrupt_configure_dt(&cfg->interrupt, GPIO_INT_DISABLE);
	if (rc < 0) {
		LOG_ERR("Can't disable interrupt: %d", rc);
	}

	return rc;
}

static int ec6601_set_mode(const struct device *dev,
			    enum fingerprint_sensor_mode mode)
{
	int rc = 0, rc_cmd;

	switch (mode) {
	case FINGERPRINT_SENSOR_MODE_DETECT:
		if (IS_ENABLED(CONFIG_HAVE_EGIS_EC6601_PRIVATE_DRIVER)) {
			fp_sensor_configure_detect();
			rc = ec6601_enable_irq(dev);
		} else {
			rc = -ENOTSUP;
		}
		break;

	case FINGERPRINT_SENSOR_MODE_LOW_POWER:
		rc = ec6601_disable_irq(dev);
		rc_cmd = ec6601_send_cmd(dev, EC6601_CMD_DEEPSLEEP);

		if (rc == 0) {
			rc = rc_cmd;
		}
		break;

	case FINGERPRINT_SENSOR_MODE_IDLE:
		rc = ec6601_disable_irq(dev);
		break;

	default:
		rc = -ENOTSUP;
	}

	return rc;
}

static int ec6601_init(const struct device *dev)
{
	const struct ec6601_cfg *cfg = dev->config;
	struct ec6601_data *data = dev->data;
	uint16_t id = 0;
	int rc;

	if (IS_ENABLED(CONFIG_HAVE_EGIS_EC6601_PRIVATE_DRIVER)) {
		/* Print the binary libfpbep.a library version. */
		LOG_INF("EGIS libfpbep.a %s\n", fp_sensor_get_version());

		/* Print the BEP version and build time of the library. */
		LOG_INF("Build information - %s\n",
			   fp_sensor_get_build_info());
	}

	data->errors = FINGERPRINT_ERROR_DEAD_PIXELS_UNKNOWN;

	int int_pin_value = gpio_pin_get_dt(&cfg->interrupt);

	rc = ec6601_get_hwid(dev, &id);
	if (rc) {
		LOG_ERR("Failed to get EGIS HWID: %d", rc);
		return rc;
	}

	if ((id >> 4) != FP_SENSOR_HWID_EGIS) {
		LOG_ERR("EGIS unknown silicon 0x%04x", id);
		return -EINVAL;
	}

	LOG_INF("EC6601 id 0x%04x\n", id);

	if (IS_ENABLED(CONFIG_HAVE_EGIS_EC6601_PRIVATE_DRIVER)) {
		rc = fp_sensor_open();
		if (rc) {
			LOG_ERR("fp_sensor_open() failed, result %d", rc);
			data->errors |= FINGERPRINT_ERROR_INIT_FAIL;
			return rc;
		}
	}

	if (int_pin_value == gpio_pin_get_dt(&cfg->interrupt)) {
		LOG_ERR("Sensor IRQ not ready");
		data->errors |= FINGERPRINT_ERROR_NO_IRQ;
	}

	ec6601_set_mode(dev, FINGERPRINT_SENSOR_MODE_LOW_POWER);

	return 0;
}

static int ec6601_deinit(const struct device *dev)
{
	int rc;

	if (!IS_ENABLED(CONFIG_HAVE_EGIS_EC6601_PRIVATE_DRIVER)) {
		return 0;
	}

	rc = fp_sensor_close();
	if (rc < 0) {
		LOG_ERR("fp_sensor_close() failed, result %d", rc);
		return rc;
	}

	return 0;
}

static int ec6601_get_info(const struct device *dev,
			    struct fingerprint_info *info)
{
	const struct ec6601_cfg *cfg = dev->config;
	struct ec6601_data *data = dev->data;
	uint16_t id = 0;
	int rc;

	/* Copy immutable sensor information to the structure. */
	memcpy(info, &cfg->info, sizeof(struct fingerprint_info));

	rc = ec6601_get_hwid(dev, &id);
	if (rc) {
		LOG_ERR("Failed to get EGIS HWID: %d", rc);
		return rc;
	}

	info->model_id = id;
	info->errors = data->errors;

	return 0;
}

static int ec6601_config(const struct device *dev, fingerprint_callback_t cb)
{
	struct ec6601_data *data = dev->data;

	data->callback = cb;

	return 0;
}

static int ec6601_maintenance(const struct device *dev, uint8_t *buf,
			       size_t size)
{
	struct ec6601_data *data = dev->data;
	struct fp_sensor_info sensor_info;
	uint64_t start;
	int rc = 0;

	if (!IS_ENABLED(CONFIG_HAVE_EGIS_EC6601_PRIVATE_DRIVER)) {
		return -ENOTSUP;
	}

	if (size < CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE)
		return -EINVAL;

	start = k_uptime_get();

	rc = fp_sensor_maintenance(buf, &sensor_info);
	LOG_INF("Maintenance took %lld ms", k_uptime_delta(&start));

	if (rc != 0) {
		/*
		 * Failure can occur if any of the fingerprint detection zones
		 * are covered (i.e., finger is on sensor).
		 */
		LOG_WRN("Failed to run maintenance: %d", rc);
		return -EFAULT;
	}

	data->errors &= ~FINGERPRINT_ERROR_DEAD_PIXELS_MASK;
	data->errors |= FINGERPRINT_ERROR_DEAD_PIXELS(
		MIN(sensor_info.num_defective_pixels,
		    FINGERPRINT_ERROR_DEAD_PIXELS_MAX));
	LOG_INF("num_defective_pixels: %d", sensor_info.num_defective_pixels);

	return 0;
}

BUILD_ASSERT(FINGERPRINT_SENSOR_SCAN_GOOD == EGIS_SENSOR_GOOD_IMAGE_QUALITY);
BUILD_ASSERT(FINGERPRINT_SENSOR_SCAN_LOW_IMAGE_QUALITY ==
	     EGIS_SENSOR_LOW_IMAGE_QUALITY);
BUILD_ASSERT(FINGERPRINT_SENSOR_SCAN_TOO_FAST == EGIS_SENSOR_TOO_FAST);
BUILD_ASSERT(FINGERPRINT_SENSOR_SCAN_LOW_SENSOR_COVERAGE ==
	     EGIS_SENSOR_LOW_COVERAGE);

static int ec6601_acquire_image(const struct device *dev,
				 enum fingerprint_capture_type capture_type,
				 uint8_t *image_buf, size_t image_buf_size)
{
	int rc;

	if (image_buf_size < CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE)
		return -EINVAL;

	rc = convert_fp_capture_type_to_egis_capture_type(capture_type);

	if (rc < 0) {
		LOG_ERR("Unsupported capture_type %d provided", capture_type);
		return rc;
	}

	if (!IS_ENABLED(CONFIG_HAVE_EGIS_EC6601_PRIVATE_DRIVER)) {
		return -ENOTSUP;
	}

	rc = fp_sensor_acquire_image_with_mode(image_buf, rc);
	if (rc < 0) {
		LOG_ERR("Failed to acquire image with capture_type %d: %d",
			capture_type, rc);
		return rc;
	}

	/*
	 * Finger status codes returned by fp_sensor_acquire_image() are
	 * synchronized with FINGERPRINT_SENSOR_* defines.
	 */
	return rc;
}

BUILD_ASSERT(FINGERPRINT_FINGER_STATE_NONE == EGIS_FINGER_NONE);
BUILD_ASSERT(FINGERPRINT_FINGER_STATE_PARTIAL == EGIS_FINGER_PARTIAL);
BUILD_ASSERT(FINGERPRINT_FINGER_STATE_PRESENT == EGIS_FINGER_PRESENT);

static int ec6601_finger_status(const struct device *dev)
{
	int rc;

	if (!IS_ENABLED(CONFIG_HAVE_EGIS_EC6601_PRIVATE_DRIVER)) {
		return -ENOTSUP;
	}

	rc = fp_sensor_finger_status();
	if (rc < 0) {
		LOG_ERR("Failed to get finger status: %d", rc);
		return rc;
	}

	/*
	 * Finger status codes returned by fp_sensor_finger_status() are
	 * synchronized with fingerprint_finger_state enum.
	 */
	return rc;
}

//static DEVICE_API(fingerprint, cros_fp_ec6601_driver_api) = {
static const struct fingerprint_driver_api cros_fp_ec6601_driver_api = {
	.init = ec6601_init,
	.deinit = ec6601_deinit,
	.config = ec6601_config,
	.get_info = ec6601_get_info,
	.maintenance = ec6601_maintenance,
	.set_mode = ec6601_set_mode,
	.acquire_image = ec6601_acquire_image,
	.finger_status = ec6601_finger_status,
};

static void ec6601_irq(const struct device *dev, struct gpio_callback *cb,
			uint32_t pins)
{
	struct ec6601_data *data =
		CONTAINER_OF(cb, struct ec6601_data, irq_cb);

	ec6601_disable_irq(data->dev);

	if (data->callback != NULL) {
		data->callback(dev);
	}
}

static int ec6601_init_driver(const struct device *dev)
{
	const struct ec6601_cfg *cfg = dev->config;
	struct ec6601_data *data = dev->data;
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
	gpio_init_callback(&data->irq_cb, ec6601_irq, BIT(cfg->interrupt.pin));
	gpio_add_callback_dt(&cfg->interrupt, &data->irq_cb);

	return 0;
}

#define EC6601_SENSOR_INFO(inst)                                      \
	{                                                              \
		.vendor_id = FOURCC('E', 'G', 'I', 'S'),               \
		.product_id = 9,                                       \
		.model_id = 1,                                         \
		.version = 1,                                          \
		.frame_size = CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE,    \
		.pixel_format = FINGERPRINT_SENSOR_V4L2_PIXEL_FORMAT(  \
			DT_DRV_INST(inst)),                            \
		.width = FINGERPRINT_SENSOR_RES_X(DT_DRV_INST(inst)),  \
		.height = FINGERPRINT_SENSOR_RES_Y(DT_DRV_INST(inst)), \
		.bpp = FINGERPRINT_SENSOR_RES_BPP(DT_DRV_INST(inst)),  \
	}

#define EGIS_EC6601_DEFINE(inst)                                                   \
	static struct ec6601_data ec6601_data_##inst;                        \
	static const struct ec6601_cfg ec6601_cfg_##inst = {                 \
		.spi = SPI_DT_SPEC_INST_GET(                                   \
			inst, SPI_OP_MODE_MASTER | SPI_WORD_SET(8), 0),        \
		.interrupt = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),           \
		.reset_pin = GPIO_DT_SPEC_INST_GET(inst, reset_gpios),         \
		/*.cs_pin = GPIO_DT_SPEC_INST_GET(inst, cs_gpios), */          \
		.info = EC6601_SENSOR_INFO(inst),                             \
	};                                                                     \
	BUILD_ASSERT(                                                          \
		CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE >=                        \
			FINGERPRINT_SENSOR_REAL_IMAGE_SIZE(DT_DRV_INST(inst)), \
		"FP image buffer size is smaller than raw image size");        \
	DEVICE_DT_INST_DEFINE(inst, ec6601_init_driver, NULL,                 \
			      &ec6601_data_##inst, &ec6601_cfg_##inst,       \
			      POST_KERNEL,                                     \
			      CONFIG_FINGERPRINT_SENSOR_INIT_PRIORITY,         \
			      &cros_fp_ec6601_driver_api)

DT_INST_FOREACH_STATUS_OKAY(EGIS_EC6601_DEFINE);
