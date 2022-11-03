/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CROS_BUTTON_H
#define ZEPHYR_INCLUDE_DRIVERS_CROS_BUTTON_H

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

struct cros_button_data {
	struct gpio_callback cb_data;
};

struct cros_button_config {
	uint32_t type;
	uint32_t debounce_us;
	struct gpio_dt_spec spec;
};

#define BUTTON_CFG_DEF(i)                                        \
	{                                                        \
		.type = DT_INST_PROP_OR(i, button_type, 0),      \
		.spec = GPIO_DT_SPEC_GET(DT_DRV_INST(i), gpios), \
		.debounce_us = DT_INST_PROP(i, debounce_us),     \
	}

__subsystem struct cros_button_api {
	int (*get_config)(const struct device *dev,
			  struct cros_button_config *cfg);
	int (*get_debounce_us)(const struct device *dev, int *debounce_us);
	int (*enable_interrupt)(const struct device *dev,
				gpio_callback_handler_t cb);
	int (*disable_interrupt)(const struct device *dev);
	int (*is_pressed)(const struct device *dev);
	int (*is_pressed_raw)(const struct device *dev);
};

/**
 * @brief Get the button configuration
 *
 * @param dev button config device instance
 * @param cfg pointer button config
 *
 * @return 0 If successful
 */
__syscall int button_get_config(const struct device *dev,
				struct cros_button_config *cfg);

static inline int z_impl_button_get_config(const struct device *dev,
					   struct cros_button_config *cfg)
{
	const struct cros_button_api *api =
		(const struct cros_button_api *)dev->api;

	if (api->get_config == NULL) {
		return -ENOSYS;
	}

	return api->get_config(dev, cfg);
}

/**
 * @brief Get button debounce time in microseconds
 *
 * @param dev button config device instance
 * @param debounce_us pointer to debounce time
 *
 * @return 0 If successful
 */
__syscall int button_get_debounce_us(const struct device *dev,
				     int *debounce_us);

static inline int z_impl_button_get_debounce_us(const struct device *dev,
						int *debounce_us)
{
	struct cros_button_api *api;

	api = (struct cros_button_api *)dev->api;
	return api->get_debounce_us(dev, debounce_us);
}

/**
 * @brief Enable interrupt for button
 *
 * @param dev button config device instance
 *
 * @return 0 If successful
 */
__syscall int button_enable_interrupt(const struct device *dev,
				      gpio_callback_handler_t cb);

static inline int z_impl_button_enable_interrupt(const struct device *dev,
						 gpio_callback_handler_t cb)
{
	struct cros_button_api *api;

	api = (struct cros_button_api *)dev->api;
	return api->enable_interrupt(dev, cb);
}

/**
 * @brief Disable interrupt for button
 *
 * @param dev button config device instance
 *
 * @return 0 If successful
 */
__syscall int button_disable_interrupt(const struct device *dev);

static inline int z_impl_button_disable_interrupt(const struct device *dev)
{
	struct cros_button_api *api;

	api = (struct cros_button_api *)dev->api;
	return api->disable_interrupt(dev);
}

/**
 * @brief Get the logical level of button press
 *
 * @param dev button config device instance
 *
 * @return int
 */
__syscall int button_is_pressed(const struct device *dev);

static inline int z_impl_button_is_pressed(const struct device *dev)
{
	struct cros_button_api *api;

	api = (struct cros_button_api *)dev->api;
	return api->is_pressed(dev);
}

/**
 * @brief Get the physical level of button press
 *
 * @param dev button config device instance
 *
 * @return int
 */
__syscall int button_is_pressed_raw(const struct device *dev);

static inline int z_impl_button_is_pressed_raw(const struct device *dev)
{
	struct cros_button_api *api;

	api = (struct cros_button_api *)dev->api;
	return api->is_pressed_raw(dev);
}

#include <syscalls/cros_button.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_CROS_BUTTON_H */
