/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPIO module for Rotor MCU */

#include "gpio.h"
#include "registers.h"
#include "util.h"

test_mockable int gpio_get_level(enum gpio_signal signal)
{
	uint32_t mask = gpio_list[signal].mask;
	int i;
	uint32_t val;

	if (mask == 0)
		return 0;

	i = GPIO_MASK_TO_NUM(mask);
	val = ROTOR_MCU_GPIO_PLR(gpio_list[signal].port);
	return (val & (1 << i)) ? 1 : 0;
}

void gpio_set_level(enum gpio_signal signal, int value)
{
	uint32_t mask = gpio_list[signal].mask;
	int i;

	if (mask == 0)
		return;

	i = GPIO_MASK_TO_NUM(mask);
	/* Enable direct writes to take effect. */
	ROTOR_MCU_GPIO_DWER(gpio_list[signal].port) |= (1 << i);

	if (value)
		ROTOR_MCU_GPIO_OLR(gpio_list[signal].port) |= (1 << i);
	else
		ROTOR_MCU_GPIO_OLR(gpio_list[signal].port) &= ~(1 << i);
}

void gpio_pre_init(void)
{
	/* What are the pre init tasks? */

	/* Need to make sure thet that the DWER has the right settings for the
	 * GPIO to enable direct write. */
}

void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags)
{
	int i;

	while(mask) {
		i = GPIO_MASK_TO_NUM(mask);
		mask &= ~(1 << i);

		/* Enable direct writes to take effect. */
		ROTOR_MCU_GPIO_DWER(port) |= (1 << i);

		/* Input/Output */
		if (flags & GPIO_OUTPUT)
			ROTOR_MCU_GPIO_PDR(port) |= (1 << i);
		else
			ROTOR_MCU_GPIO_PDR(port) &= ~(1 << i);

		/* Pull Up / Pull Down */
		if (flags & GPIO_PULL_UP) {
			ROTOR_MCU_GPIO_PCFG(port, i) |= (1 << 14);
		} else if (flags & GPIO_PULL_DOWN) {
			ROTOR_MCU_GPIO_PCFG(port, i) |= (1 << 13);
		} else {
			/* No pull up/down */
			ROTOR_MCU_GPIO_PCFG(port, i) &= ~(3 << 14);
		}

		/* Edge vs. Level Interrupts */
		if (flags & (GPIO_INT_F_RISING | GPIO_INT_F_FALLING))
			ROTOR_MCU_GPIO_IMR(port) &= (1 << i);
		else
			ROTOR_MCU_GPIO_IMR(port) |= (1 << i);

		/* Interrupt polarity */
		if (flags & (GPIO_INT_F_RISING | GPIO_INT_F_HIGH))
			ROTOR_MCU_GPIO_HRIPR(port) |= (1 << i);
		else
			ROTOR_MCU_GPIO_HRIPR(port) &= ~(1 << i);

		if (flags & (GPIO_INT_F_FALLING | GPIO_INT_F_LOW))
			ROTOR_MCU_GPIO_LFIPR(port) |= (1 << i);
		else
			ROTOR_MCU_GPIO_LFIPR(port) &= (1 << i);

		/* Set level */
		if (flags & GPIO_OUTPUT) {
			if (flags & GPIO_HIGH) {
				ROTOR_MCU_GPIO_OLR(port) |= (1 << i);
			} else if (flags & GPIO_LOW) {
				ROTOR_MCU_GPIO_OLR(port) &= ~(1 << i);
			}
		}

		/* No analog support. */
	};
}

void gpio_set_alternate_function(uint32_t port, uint32_t mask, int func)
{
	int i;

	while (mask) {
		i = GPIO_MASK_TO_NUM(mask);
		mask &= ~(1 << i);

		ROTOR_MCU_GPIO_PCFG(port, i) &= ~0x7;
		if (func > 0)
			ROTOR_MCU_GPIO_PCFG(port, i) |= (func & 0x7);
	};
}
