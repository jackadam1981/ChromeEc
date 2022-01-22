/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <init.h>
#include <kernel.h>
#include <logging/log.h>

#include "gpio.h"
#include "gpio/gpio.h"
#include "cros_version.h"

LOG_MODULE_REGISTER(gpio_int, LOG_LEVEL_ERR);

/* Maps platform/ec gpio callback information */
struct gpio_signal_callback {
	/* The platform/ec gpio_signal */
	const enum gpio_signal signal;
	/* IRQ handler from platform/ec code */
	void (*const irq_handler)(enum gpio_signal signal);
	/* Interrupt-related gpio flags */
	const gpio_flags_t flags;
};

/*
 * Each zephyr project should define EC_CROS_GPIO_INTERRUPTS in their gpio_map.h
 * file if there are any interrupts that should be registered.  The
 * corresponding handler will be declared here, which will prevent
 * needing to include headers with complex dependencies in gpio_map.h.
 *
 * EC_CROS_GPIO_INTERRUPTS is a space-separated list of GPIO_INT items.
 */

/*
 * Validate interrupt flags are valid for the Zephyr GPIO driver.
 */
#define GPIO_INT(sig, f, cb)                       \
	BUILD_ASSERT(VALID_GPIO_INTERRUPT_FLAG(f), \
		     STRINGIFY(sig) " is not using Zephyr interrupt flags");
#ifdef EC_CROS_GPIO_INTERRUPTS
	EC_CROS_GPIO_INTERRUPTS
#endif
#undef GPIO_INT

/*
 * Create unique enum values for each GPIO_INT entry, which also sets
 * the ZEPHYR_GPIO_INT_COUNT value.
 */
#define ZEPHYR_GPIO_INT_ID(sig) INT_##sig
#define GPIO_INT(sig, f, cb) ZEPHYR_GPIO_INT_ID(sig),
enum zephyr_gpio_int_id {
#ifdef EC_CROS_GPIO_INTERRUPTS
	EC_CROS_GPIO_INTERRUPTS
#endif
	ZEPHYR_GPIO_INT_COUNT,
};
#undef GPIO_INT

/* Create prototypes for each GPIO IRQ handler */
#define GPIO_INT(sig, f, cb) void cb(enum gpio_signal signal);
#ifdef EC_CROS_GPIO_INTERRUPTS
EC_CROS_GPIO_INTERRUPTS
#endif
#undef GPIO_INT

/*
 * The Zephyr gpio_callback data needs to be updated at runtime, so allocate
 * into uninitialized data (BSS). The constant data pulled from
 * EC_CROS_GPIO_INTERRUPTS is stored separately in the gpio_interrupts[] array.
 */
static struct gpio_callback zephyr_gpio_callbacks[ZEPHYR_GPIO_INT_COUNT];

#define ZEPHYR_GPIO_CALLBACK_TO_INDEX(cb)                 \
	(int)(((int)(cb) - (int)&zephyr_gpio_callbacks) / \
	      sizeof(struct gpio_callback))

#define GPIO_INT(sig, f, cb)       \
	{                          \
		.signal = sig,     \
		.flags = f,        \
		.irq_handler = cb, \
	},
const static struct gpio_signal_callback
	gpio_interrupts[ZEPHYR_GPIO_INT_COUNT] = {
#ifdef EC_CROS_GPIO_INTERRUPTS
	EC_CROS_GPIO_INTERRUPTS
#endif
#undef GPIO_INT
	};

/* The single zephyr gpio handler that routes to appropriate platform/ec cb */
static void gpio_handler_shim(const struct device *port,
			      struct gpio_callback *cb, gpio_port_pins_t pins)
{
	int callback_index = ZEPHYR_GPIO_CALLBACK_TO_INDEX(cb);
	const struct gpio_signal_callback *const gpio =
		&gpio_interrupts[callback_index];

	/* Call the platform/ec gpio interrupt handler */
	gpio->irq_handler(gpio->signal);
}

/**
 * get_interrupt_from_signal() - Translate a gpio_signal to the
 * corresponding gpio_signal_callback
 *
 * @signal		The signal to convert.
 *
 * Return: A pointer to the corresponding entry in gpio_interrupts, or
 * NULL if one does not exist.
 */
const static struct gpio_signal_callback *
get_interrupt_from_signal(enum gpio_signal signal)
{
	if (!gpio_is_implemented(signal))
		return NULL;

	for (size_t i = 0; i < ARRAY_SIZE(gpio_interrupts); i++) {
		if (gpio_interrupts[i].signal == signal)
			return &gpio_interrupts[i];
	}

	LOG_ERR("No interrupt defined for GPIO %s", gpio_get_name(signal));
	return NULL;
}
