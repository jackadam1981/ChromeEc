/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPIO module for Chrome EC */

#include "clock.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "switch.h"
#include "task.h"
#include "timer.h"
#include "util.h"

void gpio_set_alternate_function(uint32_t port, uint32_t mask, int func)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
}

test_mockable int gpio_get_level(enum gpio_signal signal)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
	return 0;
}

void gpio_set_level(enum gpio_signal signal, int value)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
}

void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
}

int gpio_enable_interrupt(enum gpio_signal signal)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
	return 0;
}

int gpio_disable_interrupt(enum gpio_signal signal)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
	return 0;
}

void gpio_pre_init(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
}
