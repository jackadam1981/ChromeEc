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
	int rpm_scale;
	uint32_t config;
};

extern const struct pwm_t pwm_channels[];

/* Constant for PWM channel without tach pin */
#define PWM_NO_TACH GPIO_COUNT

/* PWM channel config flags */
#define PWM_AUTO_RESTART    0x8000
#define PWM_RPM_AVG_2       0x0010
#define PWM_RPM_AVG_4       0x0020
#define PWM_RPM_AVG_8       0x0030
#define PWM_PULSE_PER_REV_1 0x0000
#define PWM_PULSE_PER_REV_2 0x0004
#define PWM_PULSE_PER_REV_4 0x0008
#define PWM_PULSE_PER_REV_8 0x000c
#define PWM_MANUAL_CONTROL  0x0001

#endif /* __CROS_EC_LM4_PWM_H */
