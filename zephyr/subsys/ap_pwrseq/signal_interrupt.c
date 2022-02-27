/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <power_signals.h>
#include <signal_interrupt.h>

#include "gpio_signal.h"
#include "gpio/gpio_int.h"

#define MY_COMPAT	intel_ap_pwrseq_interrupt

#if HAS_INTERRUPT_SIGNALS

/*
 * Configuration for named interrupts power signals
 */
struct int_config {
	uint8_t int_enum;
	uint8_t gpio_enum;
	uint8_t no_enable;
};

#define INIT_CONFIG(id)							\
	{								\
		.int_enum = GPIO_INT_ENUM(DT_PHANDLE(id, interrupt)),	\
		.gpio_enum =						\
		  GPIO_SIGNAL(DT_PHANDLE(DT_PHANDLE(id, interrupt), irq_pin)),\
		.no_enable = DT_PROP(id, no_enable),			\
	 },

const static struct int_config config[] = {
DT_FOREACH_STATUS_OKAY(MY_COMPAT, INIT_CONFIG)
};

int power_signal_interrupt_enable_int(enum pwr_sig_interrupt index)
{
	if (index < 0 || index >= ARRAY_SIZE(config)) {
		return -EINVAL;
	}
	return gpio_enable_dt_interrupt(gpio_interrupt_get_config(
		config[index].int_enum));
}

int power_signal_interrupt_disable_int(enum pwr_sig_interrupt index)
{
	if (index < 0 || index >= ARRAY_SIZE(config)) {
		return -EINVAL;
	}
	return gpio_disable_dt_interrupt(gpio_interrupt_get_config(
		config[index].int_enum));
}

int power_signal_interrupt_get(enum pwr_sig_interrupt index)
{
	if (index < 0 || index >= ARRAY_SIZE(config)) {
		return -EINVAL;
	}
	return gpio_pin_get_dt(gpio_get_dt_spec(config[index].gpio_enum));
}

void power_signal_gpio_interrupt(enum gpio_signal signal)
{
	power_signal_interrupt();
}

void power_signal_interrupt_init(void)
{
	for (int i = 0; i < ARRAY_SIZE(config); i++) {
		/*
		 * Enable all interrupts except for
		 * those marked as 'no_enable'.
		 */
		if (!config[i].no_enable) {
			power_signal_interrupt_enable_int(i);
		}
	}
}

#endif /*  HAS_INTERRUPT_SIGNALS */
