/*
 * Copyright 2022 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT cros_vcmp_mock

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(SENSOR_GENERIC_EMUL, CONFIG_SENSOR_LOG_LEVEL);

struct vcmp_mock_data {
};

struct vcmp_mock_config {
};

static int vcmp_mock_sample_fetch(const struct device *dev,
				    enum sensor_channel chan)
{
	return -ENOTSUP;
}

static int vcmp_mock_channel_get(const struct device *dev,
				   enum sensor_channel chan,
				   struct sensor_value *val)
{
	return -ENOTSUP;
}

static const struct sensor_driver_api vcmp_mock_driver_api = {
	.sample_fetch = vcmp_mock_sample_fetch,
	.channel_get = vcmp_mock_channel_get,
};

int vcmp_mock_init(const struct device *dev)
{
	//const struct vcmp_mock_config *cfg = dev->config;
	//struct vcmp_mock_data *data = dev->data;

	return 0;
}

#define VCMP_MOCK_INST(inst) \
static struct vcmp_mock_data vcmp_mock_data_##inst; \
static const struct vcmp_mock_config vcmp_mock_config_##inst = { \
}; \
SENSOR_DEVICE_DT_INST_DEFINE(inst, vcmp_mock_init, NULL, \
			     &vcmp_mock_data_##inst, \
			     &vcmp_mock_config_##inst, POST_KERNEL, \
			     CONFIG_SENSOR_INIT_PRIORITY, &vcmp_mock_driver_api);

DT_INST_FOREACH_STATUS_OKAY(VCMP_MOCK_INST)
