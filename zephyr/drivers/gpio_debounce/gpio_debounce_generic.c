/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "gpio_debounce_common.h"
#include "drivers/gpio_debounce.h"
#include "gpio/gpio.h"
#include "gpio/gpio_int.h"

LOG_MODULE_REGISTER(gpio_debounce_generic, LOG_LEVEL_ERR);

#define DT_DRV_COMPAT generic_gpio_debounce

static int gpio_debounce_generic_init(const struct device *dev)
{
	struct gpio_debounce_data *data =
		(struct gpio_debounce_data *)dev->data;

	data->dev = dev;
	data->pin_state = -1;
	data->is_stable = 0;

	return 0;
}

static const struct gpio_debounce_api gpio_debounce_generic_api = {
	.get_config = gpio_debounce_common_get_cfg,
	.get_debounce_us = gpio_debounce_common_get_debounce_us,
	.enable_interrupt = gpio_debounce_common_enable_interrupt,
	.disable_interrupt = gpio_debounce_common_disable_interrupt,
	.get_pin = gpio_debounce_common_get_pin,
	.get_pin_raw = gpio_debounce_common_get_pin_raw,
};

#define GPIO_DEBOUNCE_INIT(i)                                                 \
	static const struct gpio_debounce_config gpio_debounce_config_##i =   \
		GPIO_DEBOUNCE_CFG_DEF(i);                                     \
	static struct gpio_debounce_data gpio_debounce_data_##i;              \
	DEVICE_DT_INST_DEFINE(i, &gpio_debounce_generic_init, NULL,           \
			      &gpio_debounce_data_##i,                        \
			      &gpio_debounce_config_##i, POST_KERNEL,         \
			      CONFIG_PLATFORM_EC_GPIO_DEBOUNCE_INIT_PRIORITY, \
			      &gpio_debounce_generic_api);

DT_INST_FOREACH_STATUS_OKAY(GPIO_DEBOUNCE_INIT)
