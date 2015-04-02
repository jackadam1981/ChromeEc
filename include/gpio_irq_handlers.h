/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_GPIO_IRQ_HANDLERS_H
#define __CROS_EC_GPIO_IRQ_HANDLERS_H

#define GPIO_HAS_IH(name, port, pin, flags, signal) signal,
void (* const gpio_irq_handlers[])(enum gpio_signal signal) = {
	#include "gpio.wrap"
};
const int gpio_last_with_ih = (sizeof(gpio_irq_handlers) / sizeof(void *)) - 1;

#endif /* __CROS_EC_GPIO_IRQ_HANDLERS_H */
