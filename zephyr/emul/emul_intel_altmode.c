/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Driver file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/intel_altmode.h>

#define DT_DRV_COMPAT intel_pd_altmode

LOG_MODULE_REGISTER(INTEL_ALTMODE, LOG_LEVEL_ERR);

struct pd_altmode_config {
	/* Shared interrupt pin in dual port solution */
	bool shared_irq;
};

struct pd_altmode_data {
	const struct device *dev;
	intel_altmode_callback isr_cb;
	bool interrupted;
	union data_status_reg status;
};

static int intel_altmode_read_status(const struct device *dev,
				     union data_status_reg *data)
{
	struct pd_altmode_data *dev_data = dev->data;

	memcpy(data, &dev_data->status, INTEL_ALTMODE_DATA_STATUS_REG_LEN);

	return 0;
}

static int intel_altmode_write_control(const struct device *dev,
				       union data_control_reg *data)
{
	struct pd_altmode_data *dev_data = dev->data;

	if (data->i2c_int_ack) {
		dev_data->interrupted = false;
	}

	return 0;
}

static bool intel_altmode_is_interrupted(const struct device *dev)
{
	struct pd_altmode_data *data = dev->data;

	return data->interrupted;
}

static void intel_altmode_set_result_cb(const struct device *dev,
					intel_altmode_callback cb)
{
	struct pd_altmode_data *dev_data = dev->data;
	const struct pd_altmode_config *cfg = data->dev->config;

	if (!cfg->shared_irq) {
		dev_data->isr_cb = cb;
	}
}

static const struct intel_altmode_driver_api intel_pd_altmode_driver_api = {
	.read_status = intel_altmode_read_status,
	.write_control = intel_altmode_write_control,
	.is_interrupted = intel_altmode_is_interrupted,
	.set_result_cb = intel_altmode_set_result_cb,
};

#define INTEL_ALTMODE_DEFINE(inst)                                        \
	static struct pd_altmode_data pd_altmode_data_##inst;             \
                                                                          \
	static const struct pd_altmode_config pd_altmode_config##inst = { \
		.shared_irq = DT_INST_PROP(inst, irq_shared),             \
	};

DT_INST_FOREACH_STATUS_OKAY(INTEL_ALTMODE_DEFINE)

int emul_pd_altmode_set_status(const struct device *dev,
			       const union data_status_reg data)
{
	struct pd_altmode_data *dev_data = dev->data;
	const struct pd_altmode_config *cfg = data->dev->config;

	memcpy(&dev_data->status, &data, INTEL_ALTMODE_DATA_STATUS_REG_LEN);

	dev_data->interrupted = true;

	if (!cfg->shared_irq && dev_data->isr_cb) {
		dev_data->isr_cb();
	}
}
