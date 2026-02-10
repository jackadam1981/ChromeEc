/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT rtk_wake

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rtk_wake, CONFIG_RTK_WAKE_LOG_LEVEL);

struct rtk_wake_config {
	struct gpio_dt_spec gpio;
};

struct rtk_wake_data {
	struct gpio_callback wake_cb;
};

static void rtk_wake_handler(const struct device *port,
			     struct gpio_callback *cb, gpio_port_pins_t pins)
{
	/* Wake up event handled by the hardware interrupt itself.
	 * This callback can be used for additional logging or actions if
	 * needed.
	 */
	LOG_INF("RTK wake detected!");
}

static int rtk_wake_init(const struct device *dev)
{
	const struct rtk_wake_config *config = dev->config;
	struct rtk_wake_data *data = dev->data;
	int ret;

	if (!gpio_is_ready_dt(&config->gpio)) {
		LOG_ERR("GPIO device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&config->gpio, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Failed to configure GPIO: %d", ret);
		return ret;
	}

	/*
	 * Configure interrupt to wake up the system.
	 * Even if the handler is empty, the interrupt will wake the SOC.
	 */
	gpio_init_callback(&data->wake_cb, rtk_wake_handler,
			   BIT(config->gpio.pin));

	ret = gpio_add_callback(config->gpio.port, &data->wake_cb);
	if (ret < 0) {
		LOG_ERR("Failed to add callback: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&config->gpio,
					      GPIO_INT_EDGE_FALLING);
	if (ret < 0) {
		LOG_ERR("Failed to configure interrupt: %d", ret);
		return ret;
	}

	LOG_INF("RTK wake initialized");

	return 0;
}

#define RTK_WAKE_INIT(inst)                                                   \
	static const struct rtk_wake_config rtk_wake_config_##inst = {        \
		.gpio = GPIO_DT_SPEC_INST_GET(inst, gpios),                   \
	};                                                                    \
                                                                              \
	static struct rtk_wake_data rtk_wake_data_##inst;                     \
                                                                              \
	DEVICE_DT_INST_DEFINE(inst, rtk_wake_init, NULL,                      \
			      &rtk_wake_data_##inst, &rtk_wake_config_##inst, \
			      POST_KERNEL, CONFIG_RTK_WAKE_INIT_PRIORITY,     \
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(RTK_WAKE_INIT)
