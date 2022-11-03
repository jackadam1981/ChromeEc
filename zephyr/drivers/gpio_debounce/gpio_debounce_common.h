/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __GPIO_DEBOUNCE_H
#define __GPIO_DEBOUNCE_H

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include "drivers/gpio_debounce.h"

int gpio_debounce_common_get_cfg(const struct device *dev,
				 struct gpio_debounce_config *cfg);
int gpio_debounce_common_get_debounce_us(const struct device *dev,
					 int *debounce_us);

int gpio_debounce_common_enable_interrupt(
	const struct device *dev, gpio_callback_handler_t gpio_debounce_cb);
int gpio_debounce_common_disable_interrupt(const struct device *dev);

int gpio_debounce_common_get_pin(const struct device *dev);
int gpio_debounce_common_get_pin_raw(const struct device *dev);

#endif /* __GPIO_DEBOUNCE_H */
