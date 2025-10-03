/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT ft_ft9865

#include "fingerprint_ft9865.h"
#include "fingerprint_ft9865_pal.h"
#include "fingerprint_ft9865_private.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

#include <drivers/fingerprint.h>
#include <fingerprint/v4l2_types.h>

// uint8_t ff_raw_buf[64 * 80 * 2]; //raw data buf
extern uint8_t ff_algo_buf[64 * 1024]; // share memory with alg to save sram

LOG_MODULE_REGISTER(cros_fingerprint, LOG_LEVEL_INF);

// TODO implement the driver

static inline int ft9865_enable_irq(const struct device *dev)
{
	const struct ft9865_cfg *cfg = dev->config;
	int rc;

	rc = gpio_pin_interrupt_configure_dt(&cfg->interrupt,
					     GPIO_INT_EDGE_TO_ACTIVE);
	if (rc < 0) {
		LOG_ERR("Can't enable interrupt: %d", rc);
	}

	return rc;
}

static inline int ft9865_disable_irq(const struct device *dev)
{
	const struct ft9865_cfg *cfg = dev->config;
	int rc;

	rc = gpio_pin_interrupt_configure_dt(&cfg->interrupt, GPIO_INT_DISABLE);
	if (rc < 0) {
		LOG_ERR("Can't disable interrupt: %d", rc);
	}

	return rc;
}

static int ft9865_set_mode(const struct device *dev,
			   enum fingerprint_sensor_mode mode)
{
	int rc = 0;

	switch (mode) {
	case FINGERPRINT_SENSOR_MODE_DETECT:
		if (IS_ENABLED(CONFIG_HAVE_FT98XX_PRIVATE_DRIVER)) {
			rc = ft_sensor_set_mode(FOCAL_SENSOR_MODE_DETECT);
			rc = ft9865_enable_irq(dev);
		} else {
			rc = -ENOTSUP;
		}
		break;

	case FINGERPRINT_SENSOR_MODE_LOW_POWER:
		if (IS_ENABLED(CONFIG_HAVE_FT98XX_PRIVATE_DRIVER)) {
			rc = ft_sensor_set_mode(FOCAL_SENSOR_MODE_LOW_POWER);
			rc = ft9865_disable_irq(dev);
		} else {
			rc = -ENOTSUP;
		}
		break;

	case FINGERPRINT_SENSOR_MODE_IDLE:
		if (IS_ENABLED(CONFIG_HAVE_FT98XX_PRIVATE_DRIVER))
			rc = ft_sensor_set_mode(FOCAL_SENSOR_MODE_IDLE);
		rc = ft9865_disable_irq(dev);
		break;

	default:
		rc = -ENOTSUP;
	}

	return rc;
}

static int ft9865_init(const struct device *dev)
{
	struct ft9865_data *data = dev->data;
	data->errors = FINGERPRINT_ERROR_DEAD_PIXELS_UNKNOWN;
	data->irq_event = 0;

	if (!IS_ENABLED(CONFIG_HAVE_FT98XX_PRIVATE_DRIVER)) {
		return 0;
	}

	sensor_param_t sensor_param = { 0 };
	sensor_param.hw_rst_func_impl = ft_sensor_hw_reset;
	sensor_param.spi_write_func_impl = ft_spi_write;
	sensor_param.spi_write_read_func_impl = ft_spi_write_then_read;
	sensor_param.delay_ms_func_impl = ft_delay_ms;
	sensor_param.raw_buf =
		ff_algo_buf + sizeof(ff_algo_buf) - 10 * 1024; // ff_raw_buf;
	ft_sensor_init(sensor_param);

	uint16_t chipid = ft_sensor_query_chipid();
	uint16_t cols = ft_sensor_query_cols();
	uint16_t rows = ft_sensor_query_rows();
	LOG_DBG("sensor id: %x, cols:%d, rows:%d", chipid, cols, rows);

	return 0;
}

static int ft9865_deinit(const struct device *dev)
{
	if (!IS_ENABLED(CONFIG_HAVE_FT98XX_PRIVATE_DRIVER)) {
		return 0;
	}

	return 0;
}

static int ft9865_get_info(const struct device *dev,
			   struct fingerprint_info *info)
{
	const struct ft9865_cfg *cfg = dev->config;
	struct ft9865_data *data = dev->data;

	/* Copy immutable sensor information to the structure. */
	memcpy(info, &cfg->info, sizeof(struct fingerprint_info));

	if (IS_ENABLED(CONFIG_HAVE_FT98XX_PRIVATE_DRIVER))
		info->model_id = ft_sensor_query_chipid();

	info->errors = data->errors;

	if ((data->irq_event == 0) &&
	    (IS_ENABLED(CONFIG_HAVE_FT98XX_PRIVATE_DRIVER)))
		info->errors |= FINGERPRINT_ERROR_NO_IRQ;

	return 0;
}

static int ft9865_config(const struct device *dev, fingerprint_callback_t cb)
{
	struct ft9865_data *data = dev->data;

	data->callback = cb;

	ft9865_enable_irq(dev);

	return 0;
}

static int ft9865_maintenance(const struct device *dev, uint8_t *buf,
			      size_t size)
{
	if (size < CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE) {
		return -EINVAL;
	}

	if (!IS_ENABLED(CONFIG_HAVE_FT98XX_PRIVATE_DRIVER)) {
		return -ENOTSUP;
	}

	return 0;
}

static int ft9865_acquire_image(const struct device *dev,
				enum fingerprint_capture_type capture_type,
				uint8_t *image_buf, size_t image_buf_size)
{
	int ret = -1;

	if (image_buf_size < CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE)
		return -EINVAL;

	if (!IS_ENABLED(CONFIG_HAVE_FT98XX_PRIVATE_DRIVER)) {
		return -ENOTSUP;
	}

	ret = ft_sensor_acquire_image_with_mode(image_buf, capture_type);

	return ret;
}

static int ft9865_finger_status(const struct device *dev)
{
	if (!IS_ENABLED(CONFIG_HAVE_FT98XX_PRIVATE_DRIVER)) {
		return -ENOTSUP;
	}

	if (ft_sensor_query_finger_status_simple() == 1) {
		LOG_DBG("FINGER_PRESENT");
		return FINGERPRINT_FINGER_STATE_PRESENT;
	} else {
		LOG_DBG("FINGER_NONE");
		return FINGERPRINT_FINGER_STATE_NONE;
	}
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

static void ft9865_irq(const struct device *dev, struct gpio_callback *cb,
		       uint32_t pins)
{
	struct ft9865_data *data = CONTAINER_OF(cb, struct ft9865_data, irq_cb);

	ft9865_disable_irq(data->dev);

	data->irq_event = 1;

	if (data->callback != NULL) {
		data->callback(dev);
	}
}

static int ft9865_init_driver(const struct device *dev)
{
	const struct ft9865_cfg *cfg = dev->config;
	struct ft9865_data *data = dev->data;
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
	gpio_init_callback(&data->irq_cb, ft9865_irq, BIT(cfg->interrupt.pin));
	gpio_add_callback_dt(&cfg->interrupt, &data->irq_cb);

	return 0;
}

#define FT9865_SENSOR_INFO(inst)                                       \
	{                                                              \
		.vendor_id = FOURCC('F', 'T', ' ', ' '),               \
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

#define FT9865_DEFINE(inst)                                                    \
	static struct ft9865_data ft9865_data_##inst;                          \
	static const struct ft9865_cfg ft9865_cfg_##inst = {                   \
		.spi = SPI_DT_SPEC_INST_GET(inst,                              \
					    SPI_OP_MODE_MASTER |               \
						    SPI_WORD_SET(8) |          \
						    SPI_TRANSFER_MSB,          \
					    0),                                \
		.interrupt = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),           \
		.reset_pin = GPIO_DT_SPEC_INST_GET(inst, reset_gpios),         \
		.info = FT9865_SENSOR_INFO(inst),                              \
	};                                                                     \
	BUILD_ASSERT(                                                          \
		CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE >=                        \
			FINGERPRINT_SENSOR_REAL_IMAGE_SIZE(DT_DRV_INST(inst)), \
		"FP image buffer size is smaller than raw image size");        \
	DEVICE_DT_INST_DEFINE(inst, ft9865_init_driver, NULL,                  \
			      &ft9865_data_##inst, &ft9865_cfg_##inst,         \
			      POST_KERNEL,                                     \
			      CONFIG_FINGERPRINT_SENSOR_INIT_PRIORITY,         \
			      &cros_fp_ft9865_driver_api)

DT_INST_FOREACH_STATUS_OKAY(FT9865_DEFINE);
