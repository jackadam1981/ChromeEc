/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "stdio.h"

#include "ec_app_main.h"
#include "host_command.h"

#include <zephyr/kernel.h>

#define BIT_TO_INDEX(x) (__builtin_ctz(x))

#define NUM_GPIO 8

// GPIO DT spec array
static const struct gpio_dt_spec gpios[NUM_GPIO] = {
	GPIO_DT_SPEC_GET(DT_NODELABEL(gpio1), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(gpio2), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(gpio3), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(gpio4), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(gpio5), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(gpio6), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(gpio7), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(gpio8), gpios)
};

static uint32_t gpio_int_cnt[NUM_GPIO] = {0};

// Callback data array
static struct gpio_callback cb_data[NUM_GPIO];

int get_gpio_dt_index(const struct device *dev, gpio_port_pins_t pin)
{
	for (int i = 0; i < ARRAY_SIZE(gpios); i++) {
		if (gpios[i].port == dev && gpios[i].pin == BIT_TO_INDEX(pin)) {
			return i;
		}
	}
	return -ENODEV;
}

void gpio_cb(const struct device *dev, struct gpio_callback *cb,
			 gpio_port_pins_t pins)
{
	int idx = get_gpio_dt_index(dev, pins);
	// printf("GPIO callback triggered dt_spec index: %d\n", idx);
	gpio_int_cnt[idx]++;
}

/** A stub main to call the real ec app main function. LCOV_EXCL_START */
int main(void)
{
	ec_app_main();

	for (int i = 0; i < NUM_GPIO; i++) {
		const struct gpio_dt_spec *g = &gpios[i];

		printf("<info> gpio%d int_cnt addr %p\n", i, gpio_int_cnt + i);

		if (!device_is_ready(g->port)) {
			printf("gpio%d is not ready!\n", i);
			continue;
		}

		gpio_pin_configure_dt(g, GPIO_INPUT);
		gpio_pin_interrupt_configure_dt(g, GPIO_INT_EDGE_BOTH);

		gpio_init_callback(&cb_data[i], gpio_cb, BIT(g->pin));
		gpio_add_callback(g->port, &cb_data[i]);
	}

	if (IS_ENABLED(CONFIG_TASK_HOSTCMD_THREAD_MAIN)) {
		host_command_main();
	} else if (IS_ENABLED(CONFIG_THREAD_MONITOR)) {
		/*
		 * Avoid returning so that the main stack is displayed by the
		 * "kernel stacks" shell command.
		 */
		k_sleep(K_FOREVER);
	}

	return 0;
}
/* LCOV_EXCL_STOP */
