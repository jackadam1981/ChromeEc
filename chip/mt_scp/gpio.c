/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPIO module */

#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "util.h"

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

	if (flags & (GPIO_INT_F_RISING | GPIO_INT_F_HIGH))
		SCP_EINT_POLARITY_SET[port] = mask;

	if (flags & (GPIO_INT_F_FALLING | GPIO_INT_F_LOW))
		SCP_EINT_POLARITY_CLR[port] = mask;
	else
		SCP_EINT_POLARITY_SET[port] = mask;

	/* Set sensitivity register on edge trigger */
	if (flags & (GPIO_INT_F_RISING | GPIO_INT_F_FALLING))
		SCP_EINT_SENS_SET[port] = mask;
	else
		SCP_EINT_SENS_CLR[port] = mask;
}

int gpio_get_flags_by_mask(uint32_t port, uint32_t mask)
{
	/* TODO(b/120167145): implement get flags */
	return 0;
}

int gpio_enable_interrupt(enum gpio_signal signal)
{
	const struct gpio_info *g = gpio_list + signal;

	if (signal >= GPIO_IH_COUNT || !g->mask)
		return EC_ERROR_INVAL;

	SCP_EINT_MASK_CLR[g->port] = g->mask;

	return EC_SUCCESS;
}

int gpio_disable_interrupt(enum gpio_signal signal)
{
	const struct gpio_info *g = gpio_list + signal;

	if (signal >= GPIO_IH_COUNT || !g->mask)
		return EC_ERROR_INVAL;

	SCP_EINT_MASK_SET[g->port] = g->mask;

	return EC_SUCCESS;
}

int gpio_clear_pending_interrupt(enum gpio_signal signal)
{
	const struct gpio_info *g = gpio_list + signal;

	if (signal >= GPIO_IH_COUNT || !g->mask)
		return EC_ERROR_INVAL;

	SCP_EINT_ACK[g->port] = g->mask;

	return EC_SUCCESS;
}

void gpio_pre_init(void)
{
	const struct gpio_info *g = gpio_list;
	int i;
	int is_warm = system_is_reboot_warm();

	for (i = 0; i < GPIO_COUNT; i++, g++) {
		int flags = g->flags;

		if (flags & GPIO_DEFAULT)
			continue;

		if (is_warm)
			flags &= ~(GPIO_LOW | GPIO_HIGH);

		gpio_set_flags_by_mask(g->port, g->mask, flags);
	}
}

void gpio_init(void)
{
	/* Enable EINT IRQ */
	task_enable_irq(SCP_IRQ_EINT);
}
DECLARE_HOOK(HOOK_INIT, gpio_init, HOOK_PRIO_DEFAULT);

/* Interrupt handler */
void __keep gpio_interrupt(void)
{
	int bit, port;
	uint32_t pending;
	enum gpio_signal signal;

	for (port = 0; port <= MAX_EINT_PORT; port++) {
		pending = SCP_EINT_STATUS[port];

		while (pending) {
			bit = get_next_bit(&pending);
			SCP_EINT_ACK[port] = (1 << bit);
			/* Skip masked gpio */
			if (SCP_EINT_MASK_GET[port] & (1 << bit))
				continue;
			/* Call handler */
			signal = port * 32 + bit;
			if (signal < GPIO_IH_COUNT)
				gpio_irq_handlers[signal](signal);
		}
	}
}
DECLARE_IRQ(SCP_IRQ_EINT, gpio_interrupt, 1);

