/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>

#include "common.h"
#include "gpio.h"
#include "hwwp.h"

bool hwwp_isasserted()
{
	/*
	 * This must be a macro conditional because one of
	 * GPIO_WP or GPIO_WP_L will not be defined at build time.
	 */
#ifdef CONFIG_WP_ACTIVE_HIGH
	return gpio_get_level(GPIO_WP);
#else
	return !gpio_get_level(GPIO_WP_L);
#endif
}