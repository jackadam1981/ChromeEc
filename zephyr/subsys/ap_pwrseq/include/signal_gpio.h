/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_SIGNAL_GPIO_H__
#define __AP_PWRSEQ_SIGNAL_GPIO_H__

/**
 * @brief Get the value of the GPIO power signal.
 *
 * @param signal The power_signal_gpios value to get.
 * @return the current value of the power signal.
 */
int power_signal_gpio_get(uint8_t gpio);

/**
 * @brief Set the output of this GPIO power signal.
 *
 * @param signal The GPIO to set.
 * @param value The output value to set it to.
 * @return 0 is successful
 * @return negative If output cannot be set.
 */
int power_signal_gpio_set(uint8_t gpio, int value);

/**
 * @brief Enable the GPIO interrupt
 *
 * @param signal The power_signal_gpios to enable.
 * @return 0 if successful
 * @return -error if failed
 */
int power_signal_gpio_enable_int(uint8_t gpio);

/**
 * @brief Disable the GPIO interrupt
 *
 * @param signal The power_signal_gpios to disable.
 * @return 0 if successful
 * @return -error if failed
 */
int power_signal_gpio_disable_int(uint8_t gpio);

/**
 * @brief Initialize the GPIOs for the power signals.
 */
void power_signal_gpio_init(void);

#endif /* __AP_PWRSEQ_SIGNAL_GPIO_H__ */
