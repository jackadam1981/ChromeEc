/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPIO module for ISH */

#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define ISH_TOTAL_GPIO_PINS 16

struct gpio_int_mapping {
	int8_t girq_id;
	int8_t port_offset;
};

test_mockable int gpio_get_level(enum gpio_signal signal)
{
	return  !!(ISH_GPIO_CTL(ISH_GPIO_GPLR) & gpio_list[signal].mask);
}

void gpio_set_level(enum gpio_signal signal, int value)
{
	if (value)
		ISH_GPIO_CTL(ISH_GPIO_GPSR) |=  gpio_list[signal].mask;
	else
		ISH_GPIO_CTL(ISH_GPIO_GPCR) |=  gpio_list[signal].mask;
}

void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags)
{
	/*TODO*/
}

int gpio_enable_interrupt(enum gpio_signal signal)
{
	if ((gpio_list[signal].mask == 0) ||
		gpio_list[signal].mask >= (1 << ISH_TOTAL_GPIO_PINS))
		return EC_SUCCESS;

	ISH_GPIO_CTL(ISH_GPIO_GIMR) |= gpio_list[signal].mask;

	return EC_SUCCESS;
}

int gpio_disable_interrupt(enum gpio_signal signal)
{
	if ((gpio_list[signal].mask == 0) ||
		gpio_list[signal].mask >= (1 << ISH_TOTAL_GPIO_PINS))
		return EC_SUCCESS;

	ISH_GPIO_CTL(ISH_GPIO_GIMR) &= ~gpio_list[signal].mask;

	return EC_SUCCESS;
}

int gpio_clear_pending_interrupt(enum gpio_signal signal)
{
	if ((gpio_list[signal].mask == 0) ||
                gpio_list[signal].mask >= (1 << ISH_TOTAL_GPIO_PINS))
		return EC_SUCCESS;

	ISH_GPIO_CTL(ISH_GPIO_GISR) |= gpio_list[signal].mask;
	return EC_SUCCESS;
}

void gpio_pre_init(void)
{
	int i;
	int flags;
	int is_warm = system_is_reboot_warm();
	const struct gpio_info *g = gpio_list;

	for (i = 0; i < GPIO_COUNT; i++, g++) {
		flags = g->flags;

	if (flags & GPIO_DEFAULT)
		continue;

	/*
	 *  *  * If this is a warm reboot, don't set the output levels or
	 *   *   * we'll shut off the AP.
	 *    *    */
	if (is_warm)
		flags &= ~(GPIO_LOW | GPIO_HIGH);

	gpio_set_flags_by_mask(g->port, g->mask, flags);

	}
}

static void gpio_init(void)
{
	task_enable_irq(ISH_GPIO_IRQ);
}


static void gpio_interrupt(void)
{
	int i;
	const struct gpio_info *g = gpio_list;
	uint32_t gisr = ISH_GPIO_CTL(ISH_GPIO_GISR);
	for (i = 0; i < GPIO_IH_COUNT; i++, g++) {
		if (gisr & g->mask)
			gpio_irq_handlers[i](i);
	}
}

DECLARE_IRQ(ISH_GPIO_IRQ, gpio_interrupt);

DECLARE_HOOK(HOOK_INIT, gpio_init, HOOK_PRIO_DEFAULT);
