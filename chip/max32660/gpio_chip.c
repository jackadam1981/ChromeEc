/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 GPIO module for Chrome EC */

#include "clock.h"
#include "console.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "switch.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "gpio_api.h"
#include "mxc_config.h"
#include "mxc_assert.h"

#define CPRINTF(format, args...) cprintf(CC_GPIO, format, ##args)
#define CPRINTS(format, args...) cprints(CC_GPIO, format, ##args)

/* 0-terminated list of GPIO base addresses */
static mxc_gpio_regs_t *gpio_bases[] = {
		MXC_GPIO0,
		0};

void gpio_set_alternate_function(uint32_t port, uint32_t mask, int func)
{
	gpio_cfg_t cfg;

	cfg.port = port; /* Configure the port */
	cfg.mask = mask; /* Use the mask */
	cfg.pad = -1;		 /* Do not configure the pad, let it default */
	switch (func) {
	case 1:
		cfg.func = GPIO_FUNC_ALT1;
		break;
	case 2:
		cfg.func = GPIO_FUNC_ALT2;
		break;
	case 3:
		cfg.func = GPIO_FUNC_ALT3;
		break;
	case 4:
		cfg.func = GPIO_FUNC_ALT4;
		break;
	default:
		cfg.func = GPIO_FUNC_IN;
		break;
	}
	GPIO_Config(&cfg);
}

test_mockable int gpio_get_level(enum gpio_signal signal)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(gpio_list[signal].port);

	return (gpio->in & gpio_list[signal].mask);
}

void gpio_set_level(enum gpio_signal signal, int value)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(gpio_list[signal].port);

	if (value) {
		gpio->out_set = gpio_list[signal].mask;
	} else {
		gpio->out_clr = gpio_list[signal].mask;
	}
}

void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags)
{
	gpio_cfg_t cfg;
	gpio_int_mode_t mode;
	gpio_int_pol_t pol;

	cfg.port = port;
	cfg.mask = mask;

	if (flags & GPIO_OUTPUT)
		cfg.func = GPIO_FUNC_OUT;
	else
		cfg.func = GPIO_FUNC_IN;

	/* Handle pullup / pulldown */
	if (flags & GPIO_PULL_UP) {
		cfg.pad = GPIO_PAD_PULL_UP;
	} else if (flags & GPIO_PULL_DOWN) {
		cfg.pad = GPIO_PAD_PULL_DOWN;
	} else {
		/* No pull up/down */
		cfg.pad = GPIO_PAD_NONE;
	}
	GPIO_Config(&cfg);

	/* Set gpio as level or edge trigger */
	if ((flags & GPIO_INT_F_HIGH) || (flags & GPIO_INT_F_LOW))
		mode = GPIO_INTERRUPT_LEVEL;
	else {
		mode = GPIO_INTERRUPT_EDGE;
	}

	/* Handle interrupting on both edges */
	if ((flags & GPIO_INT_F_RISING) &&
			(flags & GPIO_INT_F_FALLING))
		pol = GPIO_INTERRUPT_BOTH;
	else {
		if (flags & GPIO_INT_F_RISING)
			pol = GPIO_INTERRUPT_RISING;
		if (flags & GPIO_INT_F_FALLING)
			pol = GPIO_INTERRUPT_FALLING;
	}
	GPIO_IntConfig(&cfg, mode, pol);

	/* Set level */
	if (flags & GPIO_HIGH)
		GPIO_OutSet(&cfg);
	else if (flags & GPIO_LOW)
		GPIO_OutClr(&cfg);
}

int gpio_enable_interrupt(enum gpio_signal signal)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(gpio_list[signal].port);

	gpio->int_en_set = gpio_list[signal].mask;
	return EC_SUCCESS;
}

int gpio_disable_interrupt(enum gpio_signal signal)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(gpio_list[signal].port);

	gpio->int_en_clr = gpio_list[signal].mask;
	return EC_SUCCESS;
}

int gpio_clear_pending_interrupt(enum gpio_signal signal)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(gpio_list[signal].port);

	gpio->int_clr = gpio_list[signal].mask;
	return EC_SUCCESS;
}

void gpio_pre_init(void)
{
	const struct gpio_info *g = gpio_list;
	int i;

	/* Mask all GPIO interrupts */
	for (i = 0; gpio_bases[i]; i++)
		gpio_bases[i]->int_en = 0;

	/* Set all GPIOs to defaults */
	for (i = 0; i < GPIO_COUNT; i++, g++) {
		int flags = g->flags;

		if (flags & GPIO_DEFAULT)
			continue;

		/* Use as GPIO, not alternate function */
		gpio_set_alternate_function(g->port, g->mask, -1);

		/* Set up GPIO based on flags */
		gpio_set_flags_by_mask(g->port, g->mask, flags);
	}
}

static void gpio_init(void)
{
	/* do nothing */
}
DECLARE_HOOK(HOOK_INIT, gpio_init, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Interrupt handlers */

/**
 * Handle a GPIO interrupt.
 *
 * @param port		GPIO port
 * @param mis		Masked interrupt status value for that port
 */
static void gpio_interrupt(int port, uint32_t mis)
{
	int i = 0;
	const struct gpio_info *g = gpio_list;

	for (i = 0; i < GPIO_IH_COUNT && mis; i++, g++) {
		if (port == g->port && (mis & g->mask)) {
			gpio_irq_handlers[i](i);
			mis &= ~g->mask;
		}
	}
}

/**
 * Handlers for each GPIO port.  These read and clear the interrupt bits for
 * the port, then call the master handler above.
 */
#define GPIO_IRQ_FUNC(irqfunc, gpiobase)                 \
	void irqfunc(void)                                     \
	{                                                      \
		mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(gpiobase); \
		uint32_t mis = gpio->int_stat;                       \
		gpio->int_clr = mis;                                 \
		gpio_interrupt(gpiobase, mis);                       \
	}

GPIO_IRQ_FUNC(__gpio_0_interrupt, PORT_0);
#undef GPIO_IRQ_FUNC
DECLARE_IRQ(EC_GPIO0_IRQn, __gpio_0_interrupt, 1);
