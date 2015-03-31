/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef CONFIG_GPIO_PORT
#define GPIO(name, port, pin, flags, signal) GPIO_##name,
#else
#define DEFINE_GPIO_ENUM
#include "gpio_macro.h"
#undef DEFINE_GPIO_ENUM
#endif

#define UNIMPLEMENTED(name) GPIO_##name,

enum gpio_signal {
	#include "gpio.wrap"
	GPIO_COUNT
};
