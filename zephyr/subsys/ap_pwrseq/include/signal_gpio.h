/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_SIGNAL_GPIO_H__
#define __AP_PWRSEQ_SIGNAL_GPIO_H__

#include <signal_interface.h>

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_IN) || \
	DT_HAS_COMPAT_STATUS_OKAY(COMPAT_OUT)

/*
 * Generate an enum for the combined input and output GPIOs.
 * Inputs go first, since only inputs can generated interrupts.
 */

#define TAG_PWR_GPIO_ENUM(name) DT_CAT(PG_, name)

#define PWR_GPIO_ENUM(id)			\
	TAG_PWR_GPIO_ENUM(PWR_SIGNAL_ENUM(id))

#define GEN_GPIO_SIGNAL_ENUM(id)	PWR_GPIO_ENUM(id),

enum power_signal_gpios {
DT_FOREACH_STATUS_OKAY(COMPAT_IN, GEN_GPIO_SIGNAL_ENUM)
DT_FOREACH_STATUS_OKAY(COMPAT_OUT, GEN_GPIO_SIGNAL_ENUM)
};

#undef GEN_GPIO_SIGNAL_ENUM

/**
 * @brief Get the value of the GPIO power signal.
 *
 * @param signal The power_signal_gpios value to get.
 * @return the current value of the power signal.
 */
int power_signal_gpio_get(enum power_signal_gpios gpio);

/**
 * @brief Set the output of this GPIO power signal.
 *
 * @param signal The GPIO to set.
 * @param value The output value to set it to.
 * @return 0 is successful
 * @return negative If output cannot be set.
 */
int power_signal_gpio_set(enum power_signal_gpios gpio, int value);

/**
 * @brief Enable the GPIO interrupt
 *
 * @param signal The power_signal_gpios to enable.
 * @return 0 if successful
 * @return -error if failed
 */
int power_signal_gpio_enable_int(enum power_signal_gpios gpio);

/**
 * @brief Disable the GPIO interrupt
 *
 * @param signal The power_signal_gpios to disable.
 * @return 0 if successful
 * @return -error if failed
 */
int power_signal_gpio_disable_int(enum power_signal_gpios gpio);

/**
 * @brief Initialize the GPIOs for the power signals.
 */
void power_signal_gpio_init(void);

#endif

#endif /* __AP_PWRSEQ_SIGNAL_GPIO_H__ */
