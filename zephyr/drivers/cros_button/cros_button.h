/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_BUTTON_H
#define __CROS_BUTTON_H

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include "drivers/gpio_debounce.h"

int cros_gpio_debounce_get_cfg(const struct device *dev,
			       struct gpio_debounce_config *cfg);
int cros_gpio_debounce_get_debounce_us(const struct device *dev,
				       int *debounce_us);

int cros_gpio_debounce_enable_interrupt(
	const struct device *dev, gpio_callback_handler_t gpio_debounce_cb);
int cros_gpio_debounce_disable_interrupt(const struct device *dev);

int cros_gpio_debounce_get_pin(const struct device *dev);
int cros_gpio_debounce_get_pin_raw(const struct device *dev);

#endif /* __CROS_BUTTON_H */
