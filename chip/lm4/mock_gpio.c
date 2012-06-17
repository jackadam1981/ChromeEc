/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Mock GPIO module for Chrome EC */

#include "board.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "power_button.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"


/* 0-terminated list of GPIO bases */
static const uint32_t gpio_bases[] = {
	LM4_GPIO_A, LM4_GPIO_B, LM4_GPIO_C, LM4_GPIO_D,
	LM4_GPIO_E, LM4_GPIO_F, LM4_GPIO_G, LM4_GPIO_H,
	LM4_GPIO_J, LM4_GPIO_K, LM4_GPIO_L, LM4_GPIO_M,
	LM4_GPIO_N, LM4_GPIO_P, LM4_GPIO_Q, 0
};


static int mock_value[15] = {0};
static int mock_gpio_im[15] = {0};


/* Find the index of a GPIO port base address (LM4_GPIO_[A-Q]); this is used by
 * the clock gating registers.  Returns the index, or -1 if no match. */
static int find_gpio_port_index(uint32_t port_base)
{
	int i;
	for (i = 0; gpio_bases[i]; i++) {
		if (gpio_bases[i] == port_base)
			return i;
	}
	return -1;
}


int gpio_pre_init(void)
{
	/* Nothing to do */
	return EC_SUCCESS;
}


static int gpio_init(void)
{
	/* Nothing to do */
	return EC_SUCCESS;
}
DECLARE_HOOK(HOOK_INIT, gpio_init, HOOK_PRIO_DEFAULT);


void gpio_set_alternate_function(int port, int mask, int func)
{
	/* Not implemented */
	return;
}


const char *gpio_get_name(enum gpio_signal signal)
{
	return gpio_list[signal].name;
}


int gpio_get_level(enum gpio_signal signal)
{
	int idx = find_gpio_port_index(gpio_list[signal].port);
	return (mock_value[idx] & gpio_list[signal].mask) ? 1 : 0;
}


int gpio_set_level(enum gpio_signal signal, int value)
{
	int idx = find_gpio_port_index(gpio_list[signal].port);
	int mask = gpio_list[signal].mask;

	if (value)
		mock_value[idx] |= mask;
	else
		mock_value[idx] &= ~mask;

	return EC_SUCCESS;
}


int gpio_set_flags(enum gpio_signal signal, int flags)
{
	/* Not implemented */
	return EC_SUCCESS;
}


int gpio_enable_interrupt(enum gpio_signal signal)
{
	const struct gpio_info *g = gpio_list + signal;
	int idx = find_gpio_port_index(g->port);

	/* Fail if no interrupt handler */
	if (!g->irq_handler)
		return EC_ERROR_UNKNOWN;

	mock_gpio_im[idx] |= g->mask;
	return EC_SUCCESS;
}


/* Find a GPIO signal by name.  Returns the signal index, or GPIO_COUNT if
 * no match. */
static enum gpio_signal find_signal_by_name(const char *name)
{
	const struct gpio_info *g = gpio_list;
	int i;

	if (!name || !*name)
		return GPIO_COUNT;

	for (i = 0; i < GPIO_COUNT; i++, g++) {
		if (!strcasecmp(name, g->name))
			return i;
	}

	return GPIO_COUNT;
}


static int command_gpio_mock(int argc, char **argv)
{
	char *e;
	int v, i;
	const struct gpio_info *g;
	int idx;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	i = find_signal_by_name(argv[1]);
	if (i == GPIO_COUNT)
		return EC_ERROR_PARAM1;
	g = gpio_list + i;
	idx = find_gpio_port_index(g->port);

	v = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	gpio_set_level(i, v);
	/* TODO trigger interrupt */
	if (g->irq_handler && (mock_gpio_im[idx] & g->mask))
		g->irq_handler(i);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(gpiomock, command_gpio_mock,
			"name <0 | 1>",
			"Mock a GPIO input",
			NULL);
