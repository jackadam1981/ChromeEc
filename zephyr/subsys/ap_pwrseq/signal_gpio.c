/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <power_signals.h>
#include <signal_interface.h>
#include <drivers/gpio.h>

#define INIT_GPIO_SPEC(id, prop)			\
	COND_CODE_1(DT_NODE_HAS_PROP(id, prop),		\
	(GPIO_DT_SPEC_GET(id, prop), ),			\
	())

const static struct gpio_dt_spec spec[] = {
DT_FOREACH_CHILD_VARGS(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	INIT_GPIO_SPEC, input_gpios)
DT_FOREACH_CHILD_VARGS(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	INIT_GPIO_SPEC, output_gpios)
};

/*
 * Interrupt configuration for GPIO inputs.
 */
struct ps_gpio_int {
	gpio_flags_t flags;
	uint8_t signal;
};

#define INIT_INT_CONFIG(id)					\
	COND_CODE_1(DT_NODE_HAS_PROP(id, input_gpios),		\
	({							\
		.flags = DT_PROP_OR(id, interrupt_flags, 0),	\
		.signal = PWR_SIGNAL_ENUM(id),			\
	  }, ),							\
	())

const static struct ps_gpio_int int_config[] = {
DT_FOREACH_CHILD(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	INIT_INT_CONFIG)
};

static struct gpio_callback int_cb[GPIO_INPUT_POWER_SIGNAL_COUNT];

int power_signal_gpio_get(enum power_signal_gpios index)
{
	return gpio_pin_get_dt(&spec[index]);
}

int power_signal_gpio_set(enum power_signal signal, int value)
{
	return gpio_pin_set_dt(&spec[signal], value);
}

void power_signal_gpio_interrupt(const struct device *port,
				 struct gpio_callback *cb,
				 gpio_port_pins_t pins)
{
	power_signal_interrupt();
}

void power_signal_gpio_init(void)
{
	int i;
	/*
	 * Configure the inputs first.
	 */
	for (i = 0; i < GPIO_INPUT_POWER_SIGNAL_COUNT; i++) {
		gpio_pin_configure_dt(&spec[i], GPIO_INPUT);
		/* If interrupt, enable it */
		if (int_config[i].flags) {
			gpio_init_callback(&int_cb[i],
					   power_signal_gpio_interrupt,
					   BIT(spec[i].pin));
			gpio_add_callback(spec[i].port, &int_cb[i]);
			gpio_pin_interrupt_configure_dt(&spec[i],
							int_config[i].flags);
		}
	}
	/*
	 * Now configure outputs.
	 */
	for (; i < ARRAY_SIZE(spec); i++) {
		gpio_pin_configure_dt(&spec[i], GPIO_OUTPUT);
	}
}
