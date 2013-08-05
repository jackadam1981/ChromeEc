/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* STM32-specific PWM module for Chrome EC */

#ifndef __CROS_EC_STM32_PWM_H
#define __CROS_EC_STM32_PWM_H

/* Data structure to define PWM channels. */
struct pwm_t {
	int tim;
	int channel;
	int active_low;
	enum gpio_signal pin;
};

extern const struct pwm_t pwm_channels[];

/* Just plain id mapping for code readability */
#define STM32_TIM(x) (x)

/* Channel ID starts from 1, let's map it to 0 for convenience */
#define STM32_TIM_CH(x) (x - 1)

#endif /* __CROS_EC_STM32_PWM_H */
