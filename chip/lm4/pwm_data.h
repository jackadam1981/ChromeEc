/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LM4-specific PWM module for Chrome EC */

#ifndef __CROS_EC_LM4_PWM_H
#define __CROS_EC_LM4_PWM_H

/* Data structure to define PWM channels. */
struct pwm_t {
	int channel;
	enum gpio_signal pwm_pin;
	int pwm_alt_func;
	enum gpio_signal tach_pin;
	int tach_alt_func;
	uint32_t config;
};

extern const struct pwm_t pwm_channels[];

/* Constant for PWM channel without tach pin */
#define PWM_NO_TACH GPIO_COUNT

#endif /* __CROS_EC_LM4_PWM_H */
