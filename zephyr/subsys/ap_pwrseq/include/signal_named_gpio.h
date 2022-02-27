/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_SIGNAL_NAMED_GPIO_H__
#define __AP_PWRSEQ_SIGNAL_NAMED_GPIO_H__

#define PWR_SIG_TAG_NAMED_GPIO	PWR_NGPIO_

/*
 * Generate enums for the named GPIOs.
 * These enums are only used internally
 * to assign an index to each signal that is specific
 * to the source.
 */

#define TAG_NGPIO(tag, name) DT_CAT(tag, name)

#define PWR_NGPIO_ENUM(id) \
	TAG_NGPIO(PWR_SIG_TAG_NAMED_GPIO, PWR_SIGNAL_ENUM(id)),

enum pwr_sig_named_gpio {
#if HAS_NAMED_GPIO_SIGNALS
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_named_gpio, PWR_NGPIO_ENUM)
#endif
	PWR_SIG_NAMED_GPIO_COUNT
};

#undef	PWR_NGPIO_ENUM
#undef	TAG_NGPIO

/**
 * @brief Get the value of the named GPIO power signal.
 *
 * @param signal The enum of the named GPIO to get.
 * @return the current value of the power signal.
 */
int power_signal_named_gpio_get(enum pwr_sig_named_gpio gpio);

/**
 * @brief Set the output of this GPIO power signal.
 *
 * @param signal The GPIO to set.
 * @param value The output value to set it to.
 * @return 0 is successful
 * @return negative If output cannot be set.
 */
int power_signal_named_gpio_set(enum pwr_sig_named_gpio gpio, int value);

#endif /* __AP_PWRSEQ_SIGNAL_NAMED_GPIO_H__ */
