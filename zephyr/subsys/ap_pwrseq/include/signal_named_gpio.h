/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_SIGNAL_NAMED_GPIO_H__
#define __AP_PWRSEQ_SIGNAL_NAMED_GPIO_H__

/**
 * @brief Get the value of the named GPIO power signal.
 *
 * @param signal The enum of the named GPIO to get.
 * @return the current value of the power signal.
 */
int power_signal_named_gpio_get(uint8_t gpio);

/**
 * @brief Set the output of this GPIO power signal.
 *
 * @param signal The GPIO to set.
 * @param value The output value to set it to.
 * @return 0 is successful
 * @return negative If output cannot be set.
 */
int power_signal_named_gpio_set(uint8_t gpio, int value);

#endif /* __AP_PWRSEQ_SIGNAL_NAMED_GPIO_H__ */
