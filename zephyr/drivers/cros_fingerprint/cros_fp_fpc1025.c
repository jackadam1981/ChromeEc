/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT fpc_fpc1025

#include "cros_fp_fpc1025.h"
#include "fpc1025_private.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <drivers/cros_fingerprint.h>

LOG_MODULE_REGISTER(cros_fingerprint, LOG_LEVEL_INF);

typedef struct fpc_bep_sensor fpc_bep_sensor_t;

typedef struct {
	const fpc_bep_sensor_t *sensor;
	uint32_t image_buffer_size;
} fpc_sensor_info_t;

extern const fpc_bep_sensor_t fpc_bep_sensor_1025;
const fpc_sensor_info_t fpc_sensor_info = {
	.sensor = &fpc_bep_sensor_1025,
	.image_buffer_size = CONFIG_FP_SENSOR_IMAGE_SIZE,
};

/* Sensor IC commands */
enum fpc1025_cmd {
	FPC1025_CMD_DEEPSLEEP = 0x2C,
	FPC1025_CMD_HW_ID = 0xFC,
};

/* The 16-bit hardware ID is 0x021y */
#define FP_SENSOR_HWID_FPC 0x021

static int fpc1025_send_cmd(const struct device *dev, uint8_t cmd)
{
	const struct fpc1025_cfg *cfg = dev->config;
	const struct spi_buf tx_buf[1] = { { .buf = &cmd, .len = 1 } };
	const struct spi_buf_set tx = { .buffers = tx_buf, .count = 1 };
	int err = spi_write_dt(&cfg->spi, &tx);

	/* Release CS line */
	spi_release_dt(&cfg->spi);

	return err;
}

static int fpc1025_get_hwid(const struct device *dev, uint16_t *id)
{
	const struct fpc1025_cfg *cfg = dev->config;
	uint8_t cmd = FPC1025_CMD_HW_ID;
	int err;

	const struct spi_buf trx_buf[2] = { { .buf = &cmd, .len = 1 },
					    { .buf = id, .len = 2 } };
	const struct spi_buf_set trx = { .buffers = trx_buf, .count = 2 };

	if (id == NULL)
		return -EINVAL;

	err = spi_transceive_dt(&cfg->spi, &trx, &trx);

	/* Release CS line */
	spi_release_dt(&cfg->spi);

	/* HWID is in big endian, so convert it CPU endianess. */
	*id = sys_be16_to_cpu(*id);

	return err;
}

static int fpc1025_enter_low_power(const struct device *dev)
{
	return fpc1025_send_cmd(dev, FPC1025_CMD_DEEPSLEEP);
}

static int fpc1025_init(const struct device *dev)
{
	struct fpc1025_data *data = dev->data;
	uint16_t id = 0;
	int rc;

	/* Print the binary libfpbep.a library version. */
	LOG_PRINTK("FPC libfpbep.a %s\n", fp_sensor_get_version());

	/* Print the BEP version and build time of the library. */
	LOG_PRINTK("Build information - %s\n", fp_sensor_get_build_info());

	data->errors = FINGERPRINT_ERROR_DEAD_PIXELS_UNKNOWN;

	rc = fpc1025_get_hwid(dev, &id);
	if (rc) {
		LOG_ERR("Failed to get FPC HWID: %d", rc);
		return rc;
	}

	if ((id >> 4) != FP_SENSOR_HWID_FPC) {
		LOG_ERR("FPC unknown silicon 0x%04x", id);
		return -EINVAL;
	}

	LOG_PRINTK("FPC1025 id 0x%04x\n", id);

	rc = fp_sensor_open();
	if (rc) {
		LOG_ERR("fp_sensor_open() failed, result %d", rc);
		data->errors |= FINGERPRINT_ERROR_INIT_FAIL;
		return rc;
	}

	fpc1025_enter_low_power(dev);

	return 0;
}

static int fpc1025_deinit(const struct device *dev)
{
	int rc;

	rc = fp_sensor_close();
	if (rc < 0) {
		LOG_ERR("fp_sensor_close() failed, result %d", rc);
		return rc;
	}

	return 0;
}

static int fpc1025_get_info(const struct device *dev,
			    struct fingerprint_info *info)
{
	const struct fpc1025_cfg *cfg = dev->config;
	struct fpc1025_data *data = dev->data;
	uint16_t id = 0;
	int rc;

	/* Copy immutable sensor information to the structure. */
	memcpy(info, &cfg->info, sizeof(struct fingerprint_info));

	rc = fpc1025_get_hwid(dev, &id);
	if (rc) {
		LOG_ERR("Failed to get FPC HWID: %d", rc);
		return rc;
	}

	info->model_id = id;
	info->errors = data->errors;

	return 0;
}

static int fpc1025_config(const struct device *dev, fingerprint_callback_t cb)
{
	struct fpc1025_data *data = dev->data;

	data->callback = cb;

	return 0;
}

static int fpc1025_maintenance(const struct device *dev, uint8_t *buf,
			       size_t size)
{
	struct fpc1025_data *data = dev->data;
	fp_sensor_info_t sensor_info;
	uint64_t start;
	int rc = 0;

	if (size < CONFIG_FP_SENSOR_IMAGE_SIZE)
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

	data->errors |=
		FINGERPRINT_ERROR_DEAD_PIXELS(sensor_info.num_defective_pixels);
	LOG_INF("num_defective_pixels: %d", sensor_info.num_defective_pixels);

	return 0;
}

static int fpc1025_configure_detect(const struct device *dev)
{
	fp_sensor_configure_detect();

	return 0;
}

static int fpc1025_acquire_image(const struct device *dev, int mode,
				 uint32_t *status, uint8_t *image, size_t size)
{
	int rc;

	if (size < CONFIG_FP_SENSOR_IMAGE_SIZE)
		return -EINVAL;

	rc = fp_sensor_acquire_image_with_mode(image, mode);
	if (rc < 0) {
		LOG_ERR("Failed to acquire image with mode %d: %d", mode, rc);
		return rc;
	}

	/*
	 * Finger status codes returned by fp_sensor_acquire_image() are
	 * synchronized with FP_SENSOR_* defines.
	 */
	*status = rc;

	return 0;
}

static int fpc1025_finger_status(const struct device *dev, uint32_t *status)
{
	int rc;

	rc = fp_sensor_finger_status();
	if (rc < 0) {
		LOG_ERR("Failed to get finger status: %d", rc);
		return rc;
	}

	/*
	 * Finger status codes returned by fp_sensor_finger_status() are
	 * synchronized with fingerprint_finger_state enum.
	 */
	*status = rc;

	return 0;
}

static const struct fingerprint_driver_api cros_fp_fpc1025_driver_api = {
	.init = fpc1025_init,
	.deinit = fpc1025_deinit,
	.config = fpc1025_config,
	.get_info = fpc1025_get_info,
	.maintenance = fpc1025_maintenance,
	.enter_low_power = fpc1025_enter_low_power,
	.configure_detect = fpc1025_configure_detect,
	.acquire_image = fpc1025_acquire_image,
	.finger_status = fpc1025_finger_status,
};

static void fpc1025_irq(const struct device *dev, struct gpio_callback *cb,
			uint32_t pins)
{
	struct fpc1025_data *data = CONTAINER_OF(cb, struct fpc1025_data, irq_cb);

	if (data->callback != NULL) {
		data->callback(dev, 0);
	}
}

static int fpc1025_init_driver(const struct device *dev)
{
	const struct fpc1025_cfg *cfg = dev->config;
	struct fpc1025_data *data = dev->data;
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

	ret = gpio_pin_interrupt_configure_dt(&cfg->interrupt,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Can't enable interrupt");
		return ret;
	}

	gpio_init_callback(&data->irq_cb, fpc1025_irq, BIT(cfg->interrupt.pin));
	gpio_add_callback_dt(&cfg->interrupt, &data->irq_cb);

	return 0;
}

#define FPC1025_DEFINE(inst)                                                   \
	static struct fpc1025_data fpc1025_data##inst;                         \
	static const struct fpc1025_cfg fpc1025_cfg##inst = {                \
		.spi = SPI_DT_SPEC_INST_GET(inst, SPI_OP_MODE_MASTER |       \
			SPI_WORD_SET(8) | SPI_HOLD_ON_CS | SPI_LOCK_ON,      \
			0),                                                  \
		.interrupt = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),         \
		.reset_pin = GPIO_DT_SPEC_INST_GET(inst, reset_gpios),       \
		.info = {                                                    \
			/* ['F', 'P', 'C', ' '] in little endian. */         \
			.vendor_id = 0x20435046,                             \
			.product_id = 9,                                     \
			.model_id = 1,                                       \
			.version = 1,                                        \
			.frame_size = CONFIG_FP_SENSOR_IMAGE_SIZE,           \
			.pixel_format =                                      \
			FINGERPRINT_SENSOR_V4L2_PIXEL_FORMAT(DT_DRV_INST(inst)),      \
			.width = FINGERPRINT_SENSOR_RES_X(DT_DRV_INST(inst)),         \
			.height = FINGERPRINT_SENSOR_RES_Y(DT_DRV_INST(inst)),        \
			.bpp = FINGERPRINT_SENSOR_RES_BPP(DT_DRV_INST(inst)),         \
		},                                                           \
	}; \
	BUILD_ASSERT(                                                          \
		CONFIG_FP_SENSOR_IMAGE_SIZE >=                                 \
			FINGERPRINT_SENSOR_REAL_IMAGE_SIZE(DT_DRV_INST(inst)), \
		"FP image buffer size is smaller than raw image size");        \
	DEVICE_DT_INST_DEFINE(0, fpc1025_init_driver, NULL,                    \
			      &fpc1025_data##inst, &fpc1025_cfg##inst,         \
			      POST_KERNEL,                                     \
			      CONFIG_CROS_FP_SENSOR_INIT_PRIORITY,             \
			      &cros_fp_fpc1025_driver_api);

DT_INST_FOREACH_STATUS_OKAY(FPC1025_DEFINE)
