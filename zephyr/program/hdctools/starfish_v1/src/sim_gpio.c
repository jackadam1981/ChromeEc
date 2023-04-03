/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "sim_gpio.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(sim);

struct sim_ctx {
	/* GPIO Context */
	struct gpio_ctx gpio;
};

static struct sim_ctx sim_gpios[] = { GPIO_LIST_CTX(sim) };

static struct gpio_ctx *find_gpio(enum GPIO_LABEL label, int idx)
{
	for (int i = 0; i < ARRAY_SIZE(sim_gpios); i++) {
		if (sim_gpios[i].gpio.label != label) {
			continue;
		}
		if (sim_gpios[i].gpio.idx != idx) {
			continue;
		}
		return &sim_gpios[i].gpio;
	}
	return NULL;
}

int read_gpio(enum GPIO_LABEL label, int idx)
{
	struct gpio_ctx *gpio = find_gpio(label, idx);
	if (gpio) {
		return gpio_pin_get_dt(&gpio->spec);
	}
	return 0;
}

int write_gpio(enum GPIO_LABEL label, int idx, bool state)
{
	struct gpio_ctx *gpio = find_gpio(label, idx);
	if (gpio) {
		gpio_pin_set_dt(&gpio->spec, state);
	}
	return 0;
}

void sim_gpio_init()
{
	for (int i = 0; i < ARRAY_SIZE(sim_gpios); i++) {
		gpio_flags_t flags = GPIO_OUTPUT;
		if (sim_gpios[i].gpio.label == GPIO_LABEL_SIM_CD) {
			flags = GPIO_INPUT;
		}
		gpio_pin_configure_dt(&sim_gpios[i].gpio.spec, flags);
	}

	// Set VSIM to 1.8v
	write_gpio(GPIO_LABEL_SIM_VCC_SEL, 0, 0);
}

