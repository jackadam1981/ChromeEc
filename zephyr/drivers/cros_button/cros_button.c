/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "drivers/gpio_debounce.h"

LOG_MODULE_REGISTER(button, LOG_LEVEL_ERR);

/* LCOV_EXCL_START */
#ifdef CONFIG_ZTEST
__weak int stub_gpio_pin_get(const struct device *d, gpio_pin_t p)
{
	LOG_DBG("Calling %s\n", __func__);
	return gpio_pin_get(d, p);
}
__weak int stub_gpio_pin_get_raw(const struct device *d, gpio_pin_t p)
{
	LOG_DBG("Calling %s\n", __func__);
	return gpio_pin_get_raw(d, p);
}

#define gpio_pin_get stub_gpio_pin_get
#define gpio_pin_get_raw stub_gpio_pin_get_raw
#endif
/* LCOV_EXCL_STOP */

int cros_gpio_debounce_get_cfg(const struct device *dev,
			       struct gpio_debounce_config *cfg)
{
	if (dev->config) {
		*cfg = *((struct gpio_debounce_config *)dev->config);
	}

	return 0;
}

int cros_gpio_debounce_get_debounce_us(const struct device *dev,
				       int *debounce_us)
{
	const struct gpio_debounce_config *cfg =
		(struct gpio_debounce_config *)dev->config;

	if (cfg) {
		*debounce_us = cfg->debounce_us;
	}

	return 0;
}

int cros_gpio_debounce_enable_interrupt(
	const struct device *dev, gpio_callback_handler_t gpio_debounce_cb)
{
	int retval = -ENODEV;
	const struct gpio_debounce_config *cfg =
		(struct gpio_debounce_config *)dev->config;
	struct gpio_callback *cb;
	gpio_flags_t flags;

	if (cfg) {
		cb = &((struct gpio_debounce_data *)dev->data)->cb_data;
		gpio_init_callback(cb, gpio_debounce_cb, BIT(cfg->spec.pin));
		gpio_add_callback(cfg->spec.port, cb);
		flags = (GPIO_INT_EDGE_BOTH | GPIO_INT_ENABLE) &
			~GPIO_INT_DISABLE;

		retval = gpio_pin_interrupt_configure(cfg->spec.port,
						      cfg->spec.pin, flags);
	}

	return retval;
}

int cros_gpio_debounce_disable_interrupt(const struct device *dev)
{
	int retval = -ENODEV;
	const struct gpio_debounce_config *cfg =
		(struct gpio_debounce_config *)dev->config;

	if (cfg) {
		retval = gpio_pin_interrupt_configure(
			cfg->spec.port, cfg->spec.pin, GPIO_INT_DISABLE);
	}

	return retval;
}

static int get_pin(const struct device *dev,
		   int (*gpio_pin_get_fn)(const struct device *, gpio_pin_t))
{
	int pressed = 0;
	const struct gpio_debounce_config *cfg =
		(struct gpio_debounce_config *)dev->config;

	if (cfg) {
		pressed = gpio_pin_get_fn(cfg->spec.port, cfg->spec.pin);
		if (pressed < 0) {
			LOG_ERR("Cannot read %s", dev->name);
			pressed = 0;
		}
	}

	return pressed;
}

int cros_gpio_debounce_get_pin(const struct device *dev)
{
	return get_pin(dev, gpio_pin_get);
}

int cros_gpio_debounce_get_pin_raw(const struct device *dev)
{
	return get_pin(dev, gpio_pin_get_raw);
}
