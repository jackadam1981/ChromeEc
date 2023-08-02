/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Driver file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#include "intel_altmode.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT intel_pd_altmode

LOG_MODULE_DECLARE(usbpd_altmode, CONFIG_USB_PD_ALTMODE_LOG_LEVEL);

static int pd_altmode_read(const struct device *dev)
{
	return 0;
}

static int pd_altmode_write(const struct device *dev)
{
	return 0;
}

static int pd_altmode_isr_enable(const struct device *dev, bool en)
{
	const struct pd_altmode_config *cfg = dev->config;

	return gpio_pin_interrupt_configure_dt(&cfg->int_gpio,
					       en ? GPIO_INT_EDGE_TO_INACTIVE :
						    GPIO_INT_DISABLE);
}

static const struct pd_altmode_driver pd_altmode_driver_api = {
	.read = pd_altmode_read,
	.write = pd_altmode_write,
	.isr_enable = pd_altmode_isr_enable,
};

static void pd_altmode_gpio_callback(const struct device *dev,
				     struct gpio_callback *cb, uint32_t pins)
{
}

static int pd_altmode_init(const struct device *dev)
{
	const struct pd_altmode_config *cfg = dev->config;
	struct pd_altmode_data *data = dev->data;
	int rv;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C is not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&cfg->int_gpio)) {
		LOG_ERR("GPIO is not ready");
		return -ENODEV;
	}

	data->dev = dev;

	/* Configure interrupt */
	rv = gpio_pin_configure_dt(&cfg->int_gpio, GPIO_INPUT);
	if (rv < 0) {
		LOG_ERR("Unable to configure GPIO");
		return rv;
	}

	gpio_init_callback(&data->gpio_cb, pd_altmode_gpio_callback,
			   BIT(cfg->int_gpio.pin));

	rv = gpio_add_callback(cfg->int_gpio.port, &data->gpio_cb);
	if (rv < 0) {
		LOG_ERR("Unable to add callback");
		return rv;
	}

	return 0;
}

#define INTEL_ALTMODE_DEFINE(inst)                                        \
	static struct pd_altmode_data pd_altmode_data_##inst;             \
                                                                          \
	static const struct pd_altmode_config pd_altmode_config##inst = { \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                        \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),       \
	};                                                                \
                                                                          \
	DEVICE_DT_INST_DEFINE(inst, pd_altmode_init, NULL,                \
			      &pd_altmode_data_##inst,                    \
			      &pd_altmode_config##inst, POST_KERNEL,      \
			      CONFIG_APPLICATION_INIT_PRIORITY,           \
			      &pd_altmode_driver_api);

DT_INST_FOREACH_STATUS_OKAY(INTEL_ALTMODE_DEFINE)
