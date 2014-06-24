/* -*- mode:c -*-
 *
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define GPIO(name, port, pin, function, signal) \
    {#name, GPIO_##port, (1 << pin), function, signal},

#define UNIMPLEMENTED(name) \
    {#name, DUMMY_GPIO_BANK, 0, 0, NULL},

/* GPIO signal list. */
const struct gpio_info gpio_list[] = {
	#include "gpio.inc"
};

BUILD_ASSERT(ARRAY_SIZE(gpio_list) == GPIO_COUNT);

#undef GPIO
#undef UNIMPLEMENTED
