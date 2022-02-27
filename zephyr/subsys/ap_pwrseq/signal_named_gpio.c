/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/gpio.h>

#include <power_signals.h>
#include <signal_named_gpio.h>

#include "gpio_signal.h"

#define MY_COMPAT	intel_ap_pwrseq_named_gpio

#if HAS_NAMED_GPIO_SIGNALS

/*
 * Configuration for named GPIOs
 */
struct named_gpio_config {
	uint8_t output;
	uint8_t gpio_enum;
};

#define INIT_CONFIG(id)						\
	{							\
		.output = DT_PROP(id, output),			\
		.gpio_enum = GPIO_SIGNAL(DT_PHANDLE(id, pin)),	\
	 },

const static struct named_gpio_config config[] = {
DT_FOREACH_STATUS_OKAY(MY_COMPAT, INIT_CONFIG)
};

int power_signal_named_gpio_get(enum pwr_sig_named_gpio index)
{
	return gpio_pin_get_dt(gpio_get_dt_spec(config[index].gpio_enum));
}

int power_signal_gpio_set(enum pwr_sig_named_gpio index, int value)
{
	const struct named_gpio_config *cp = &config[index];

	if (!cp->output) {
		return -EINVAL;
	}
	return gpio_pin_set_dt(gpio_get_dt_spec(cp->gpio_enum), value);
}

#endif /*  HAS_NAMED_GPIO_SIGNALS */
