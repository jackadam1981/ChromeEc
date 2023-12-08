/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_ec_fingerprint_sensor_sim

#include "fingerprint_sensor_sim.h"

#include <drivers/fingerprint.h>
#include <fingerprint/v4l2_types.h>

static int fp_simulator_init(const struct device *dev)
{
	return 0;
}

static int fp_simulator_deinit(const struct device *dev)
{
	return 0;
}

static int fp_simulator_get_info(const struct device *dev,
			    struct fingerprint_info *info)
{
	const struct fp_simulator_cfg *cfg = dev->config;
	struct fp_simulator_data *data = dev->data;

	/* Copy immutable sensor information to the structure. */
	memcpy(info, &cfg->info, sizeof(struct fingerprint_info));

	info->errors = data->errors;

	return 0;
}

static int fp_simulator_config(const struct device *dev, fingerprint_callback_t cb)
{
	struct fp_simulator_data *data = dev->data;

	data->callback = cb;

	return 0;
}

static int fp_simulator_maintenance(const struct device *dev, uint8_t *buf,
			       size_t size)
{
	return 0;
}

static int fp_simulator_set_mode(const struct device *dev,
			    enum fingerprint_sensor_mode mode)
{
	return 0;
}

static int fp_simulator_acquire_image(const struct device *dev, int mode,
				 uint8_t *image_buf, size_t image_buf_size)
{
	return 0;
}

static int fp_simulator_finger_status(const struct device *dev)
{
	return 0;
}

static const struct fingerprint_driver_api fp_simulator_driver_api = {
	.init = fp_simulator_init,
	.deinit = fp_simulator_deinit,
	.config = fp_simulator_config,
	.get_info = fp_simulator_get_info,
	.maintenance = fp_simulator_maintenance,
	.set_mode = fp_simulator_set_mode,
	.acquire_image = fp_simulator_acquire_image,
	.finger_status = fp_simulator_finger_status,
};

static int fp_simulator_init_driver(const struct device *dev)
{
	return 0;
}

#define FP_SIMULATOR_SENSOR_INFO(inst)                                    \
	{                                                                 \
		.vendor_id = FOURCC('C', 'r', 'O', 'S'),                  \
		.product_id = 0,                                          \
		.model_id = 0,                                            \
		.version = 0,                                             \
		.frame_size = FINGERPRINT_SENSOR_REAL_IMAGE_SIZE(         \
			DT_DRV_INST(inst)),                               \
		.pixel_format = FINGERPRINT_SENSOR_V4L2_PIXEL_FORMAT(     \
			DT_DRV_INST(inst)),                               \
		.width = FINGERPRINT_SENSOR_RES_X(DT_DRV_INST(inst)),     \
		.height = FINGERPRINT_SENSOR_RES_Y(DT_DRV_INST(inst)),    \
		.bpp = FINGERPRINT_SENSOR_RES_BPP(DT_DRV_INST(inst)),     \
	}

#define FP_SIMULATOR_DEFINE(inst)                                              \
	static struct fp_simulator_data fp_simulator_data_##inst;              \
	static const struct fp_simulator_cfg fp_simulator_cfg_##inst = {       \
		.info = FP_SIMULATOR_SENSOR_INFO(inst),                        \
	};                                                                     \
	DEVICE_DT_INST_DEFINE(inst, fp_simulator_init_driver, NULL,            \
			      &fp_simulator_data_##inst,                       \
			      &fp_simulator_cfg_##inst, POST_KERNEL,           \
			      CONFIG_FINGERPRINT_SENSOR_INIT_PRIORITY,         \
			      &fp_simulator_driver_api)

DT_INST_FOREACH_STATUS_OKAY(FP_SIMULATOR_DEFINE);
