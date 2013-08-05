/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM module for Chrome EC */

#ifndef __CROS_EC_PWM_H
#define __CROS_EC_PWM_H

#include "common.h"

/**
 * Enable/disable a PWM channel.
 */
void pwm_enable(enum pwm_channel ch, int enabled);

/**
 * Get PWM channel enabled status.
 */
int pwm_get_enabled(enum pwm_channel ch);

/**
 * Set PWM channel duty cycle (0-100).
 */
void pwm_set_duty(enum pwm_channel ch, int percent);

/**
 * Get PWM channel duty cycle.
 */
int pwm_get_duty(enum pwm_channel ch);

/**
 * Enable/disable RPM control logic.
 *
 * @param enabled	Enable (1) or disable (0) RPM control loop; when
 *			disabled, duty cycle will be used.
 */
void pwm_set_rpm_mode(enum pwm_channel ch, int enabled);

/**
 * Get RPM control logic enabled status.
 */
int pwm_get_rpm_mode(enum pwm_channel ch);

/**
 * Set target RPM.
 */
void pwm_set_target_rpm(enum pwm_channel ch, int rpm);

/**
 * Get target RPM.
 */
int pwm_get_target_rpm(enum pwm_channel ch);

/**
 * Get current RPM.
 */
int pwm_get_rpm(enum pwm_channel ch);

/**
 * Return non-zero if RPM control is enabled but target is stalled.
 */
int pwm_is_stalled(enum pwm_channel ch);

#endif  /* __CROS_EC_PWM_H */
