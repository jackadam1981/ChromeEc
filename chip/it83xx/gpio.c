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

/*
 * Array of base addresses for the control register of a GPIO port. The
 * argument should be a GPIO port (e.g. GPIO_A).
 */
static /*const*/ uint32_t gpio_ctrl[] = {
	0, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0xA0
};

void gpio_set_alternate_function(uint32_t port, uint32_t mask, int func)
{
	uint32_t pin = 0;

	/* For each bit high in the mask, set that pin to use alt. func. */
	while (mask > 0) {
		/* 
		 * If func is non-negative, set for alternate function.
		 * Otherwise, turn the pin into an input as it's default.
		 */
		if ((mask & 1) && func >= 0)
			IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) &= ~0xc0;
		else if ((mask & 1) && func < 0)
			IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) =
			(IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) | 0x80) & ~0x40;

		pin++;
		mask >>= 1;
	}
}

test_mockable int gpio_get_level(enum gpio_signal signal)
{
	return (IT83XX_GPIO_DATA(gpio_list[signal].port) &
			gpio_list[signal].mask) ? 1 : 0;
}

void gpio_set_level(enum gpio_signal signal, int value)
{
	if (value)
		IT83XX_GPIO_DATA(gpio_list[signal].port) |=
				 gpio_list[signal].mask;
	else
		IT83XX_GPIO_DATA(gpio_list[signal].port) &=
				~gpio_list[signal].mask;
}

void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags)
{
	uint32_t pin = 0;
	uint32_t mask_copy = mask;

	/*
	 * Select open drain first, so that we don't glitch the signal
	 * when changing the line to an output.
	 */

	if (flags & GPIO_OPEN_DRAIN)
		IT83XX_GPIO_GPOT(port) |= mask;
	else
		IT83XX_GPIO_GPOT(port) &= ~0x2;

	/* For each bit high in the mask, set input/output and pullup/down. */
	while (mask_copy > 0) {
		if (mask_copy & 1) {
			/* Set input or output. */
			if (flags & GPIO_OUTPUT)
				IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) =
				(IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) | 0x40)
				& ~0x80;
			else
				IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) =
				(IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) | 0x80)
				& ~0x40;

			/* Handle pullup / pulldown */
			if (flags & GPIO_PULL_UP) {
				IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) =
				(IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) | 0x04)
				& ~0x02;
			} else if (flags & GPIO_PULL_DOWN) {
				IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) =
				(IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) | 0x02)
				& ~0x04;
			} else {
				/* No pull up/down */
				IT83XX_GPIO_CTRL(gpio_ctrl[port], pin) &= ~0x06;
			}
		}

		pin++;
		mask_copy >>= 1;
	}


	/* TODO(crosbug.com/p/23575): fill in interrupt flags */


	/* If output, set level. */
	if (flags & GPIO_OUTPUT) {
		if (flags & GPIO_HIGH)
			IT83XX_GPIO_DATA(port) |= mask;
		else if (flags & GPIO_LOW)
			IT83XX_GPIO_DATA(port) &= ~mask;
	}
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
	const struct gpio_info *g = gpio_list;
	int flags;
	int i;

	for (i=0; i<GPIO_COUNT; i++, g++) {
		flags = g->flags;

		if (flags & GPIO_DEFAULT)
			continue;

		/* Set up GPIO based on flags */
		gpio_set_flags_by_mask(g->port, g->mask, flags);
	}
}
