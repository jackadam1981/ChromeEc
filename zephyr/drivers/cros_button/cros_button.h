/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_BUTTON_H
#define __CROS_BUTTON_H

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include "drivers/cros_button.h"

int cros_button_get_cfg(const struct device *dev,
			struct gpio_debounce_config *cfg);
int cros_button_get_debounce_us(const struct device *dev, int *debounce_us);

int cros_button_enable_interrupt(const struct device *dev,
				 gpio_callback_handler_t button_cb);
int cros_button_disable_interrupt(const struct device *dev);

int cros_button_is_pressed(const struct device *dev);
int cros_button_is_pressed_raw(const struct device *dev);

#endif /* __CROS_BUTTON_H */
