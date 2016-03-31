/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPIO module for ISH3.0 */

#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

struct gpio_int_mapping {
	int8_t girq_id;
	int8_t port_offset;
};

/* Mapping from GPIO port to GIRQ info */
static const struct gpio_int_mapping int_map[0] = {
};

void gpio_set_alternate_function(uint32_t port, uint32_t mask, int func)
{
}

test_mockable int gpio_get_level(enum gpio_signal signal)
{
	return 0;
}

void gpio_set_level(enum gpio_signal signal, int value)
{
}

void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags)
{
}

int gpio_enable_interrupt(enum gpio_signal signal)
{
	return 0;
}

int gpio_disable_interrupt(enum gpio_signal signal)
{
	return 0;
}

void gpio_pre_init(void)
{
}


static void gpio_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, gpio_init, HOOK_PRIO_DEFAULT);

