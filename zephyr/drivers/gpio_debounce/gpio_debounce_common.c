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

LOG_MODULE_REGISTER(button, LOG_LEVEL_DBG);

int gpio_debounce_common_get_cfg(const struct device *dev,
				 struct gpio_debounce_config *cfg)
{
	if (dev->config) {
		*cfg = *((struct gpio_debounce_config *)dev->config);
	}

	return 0;
}

int gpio_debounce_common_get_debounce_us(const struct device *dev,
					 int *debounce_us)
{
	const struct gpio_debounce_config *cfg =
		(struct gpio_debounce_config *)dev->config;

	if (cfg) {
		*debounce_us = cfg->debounce_us;
	}

	return 0;
}

/**
 * Handle debounced gpio pin state.
 */
static void gpio_debounce_change_deferred(struct k_work *work)
{
	struct gpio_debounce_data *data =
		CONTAINER_OF(work, struct gpio_debounce_data, work);
	const struct device *dev = data->dev;
	const struct gpio_debounce_config *cfg =
		(struct gpio_debounce_config *)dev->config;

	const int new_pressed = gpio_debounce_get_pin_raw(dev);

	LOG_DBG("gpio_change_deferred %s pin_state=%d, new_pressed=%d",
		dev->name, data->pin_state, new_pressed);

	/* If gpio hasn't changed state, nothing to do */
	if (new_pressed == data->pin_state) {
		data->is_stable = 1;
		return;
	}

	data->pin_state = new_pressed;
	data->is_stable = 1;

	LOG_DBG("Calling callback %s %d", dev->name, new_pressed);
	data->callback(dev, &data->cb_data, BIT(cfg->spec.pin));
}

static void gpio_debounce_change_call_deferred(struct gpio_debounce_data *data,
					       uint32_t usec)
{
	int rv;

	k_work_init_delayable(&data->work, gpio_debounce_change_deferred);

	rv = k_work_schedule(&data->work, K_USEC(usec));
	if (rv == 0) {
		rv = k_work_reschedule(&data->work, K_USEC(usec));
	}
	__ASSERT(rv >= 0, "Set wake mask work queue error");
}

static void gpio_debounce_interrupt(const struct device *dev,
				    struct gpio_callback *cbdata, uint32_t pins)
{
	ARG_UNUSED(dev); /* This is a pointer to GPIO device, use dev pointer in
			  * cbdata for pointer to gpio_debounce device node
			  */
	struct gpio_debounce_data *data =
		CONTAINER_OF(cbdata, struct gpio_debounce_data, cb_data);
	const int new_pressed = gpio_debounce_get_pin_raw(data->dev);

	LOG_DBG("name: %s, new_pressed=%d", data->dev->name, new_pressed);
	data->is_stable = 0;
	gpio_debounce_change_call_deferred(data, data->debounce_us);
}

int gpio_debounce_common_enable_interrupt(
	const struct device *dev, gpio_callback_handler_t gpio_debounce_cb)
{
	int retval = -ENODEV;
	const struct gpio_debounce_config *cfg =
		(struct gpio_debounce_config *)dev->config;
	struct gpio_debounce_data *data =
		(struct gpio_debounce_data *)dev->data;
	struct gpio_callback *cb;
	gpio_flags_t flags;

	if (cfg) {
		cb = &data->cb_data;
		data->callback = gpio_debounce_cb;
		data->debounce_us = cfg->debounce_us;

		gpio_init_callback(cb, gpio_debounce_interrupt,
				   BIT(cfg->spec.pin));
		gpio_add_callback(cfg->spec.port, cb);
		flags = (GPIO_INT_EDGE_BOTH | GPIO_INT_ENABLE) &
			~GPIO_INT_DISABLE;

		retval = gpio_pin_interrupt_configure(cfg->spec.port,
						      cfg->spec.pin, flags);
	}

	return retval;
}

int gpio_debounce_common_disable_interrupt(const struct device *dev)
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

int gpio_debounce_common_get_pin(const struct device *dev)
{
	return get_pin(dev, gpio_pin_get);
}

int gpio_debounce_common_get_pin_raw(const struct device *dev)
{
	return get_pin(dev, gpio_pin_get_raw);
}
