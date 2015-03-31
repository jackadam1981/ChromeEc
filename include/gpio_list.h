/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio_signal.h"

#ifdef CONFIG_GPIO_PORT
#ifdef CONFIG_COMMON_GPIO_SHORTNAMES
#define GPIO(name, port, pin, flags)			\
	{#port#pin, GPIO_##port, (1 << pin), flags},
#else
#define GPIO(name, port, pin, flags)			\
	{#name, GPIO_##port, (1 << pin), flags},
#endif
#define GPIO_INT(name, port, pin, flags, signal)	\
	GPIO(name, port, pin, flags)
#else /* CONFIG_GPIO_PORT */
#ifdef CONFIG_COMMON_GPIO_SHORTNAMES
#define GPIO(name, gpionum, flags) \
        {#gpionum, (gpionum/10), (1<<(gpionum%10)), flags},
#else
#define GPIO(name, gpionum, flags) \
        {#name, (gpionum/10), (1<<(gpionum%10)), flags},
#endif
#define GPIO_INT(name, gpionum, flags, signal)	\
	GPIO(name, gpionum, flags)
#endif /* CONFIG_GPIO_PORT */

#define UNIMPLEMENTED(name) \
	{#name, DUMMY_GPIO_BANK, 0, GPIO_DEFAULT},

/* GPIO signal list. */
const struct gpio_info gpio_list[] = {
	#include "gpio.wrap"
};

BUILD_ASSERT(ARRAY_SIZE(gpio_list) == GPIO_COUNT);

/*
 * Construct the gpio_alt_funcs array.  This array is used by gpio_config_module
 * to enable and disable GPIO alternate functions on a module by module basis.
 */
#ifdef CONFIG_GPIO_PORT
#define ALTERNATE(port, mask, function, module, flags)	\
	{GPIO_##port, mask, function, module, flags},
#else
#define ALTERNATE(pinbase, mask, function, module, flags)	\
	{ pinbase, mask, function, module, flags},
#endif

const struct gpio_alt_func gpio_alt_funcs[] = {
	#include "gpio.wrap"
};

const int gpio_alt_funcs_count = ARRAY_SIZE(gpio_alt_funcs);

/* GPIO Interrupt Handlers */
#ifdef CONFIG_GPIO_PORT
#define GPIO_INT(name, port, pin, flags, signal) signal,
#else
#define GPIO_INT(name, gpionum, flags, signal) signal,
#endif
void (* const gpio_irq_handlers[])(enum gpio_signal signal) = {
	#include "gpio.wrap"
};
const int gpio_ih_count = ARRAY_SIZE(gpio_irq_handlers);

/*
 * ALL GPIOs with interrupt handlers must be declared at the top of the gpio.inc
 * file.
 */
#ifdef CONFIG_GPIO_PORT
#define GPIO_INT(name, port, pin, flags, signal)	\
	BUILD_ASSERT(GPIO_##name < ARRAY_SIZE(gpio_irq_handlers));
#else
#define GPIO_INT(name, gpionum, flags, signal)	\
	BUILD_ASSERT(GPIO_##name < ARRAY_SIZE(gpio_irq_handlers));
#endif
#include "gpio.wrap"
