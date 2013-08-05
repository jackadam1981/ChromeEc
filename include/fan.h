/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM control module for Chromebook fans */

#ifndef __CROS_EC_FAN_H
#define __CROS_EC_FAN_H

/**
 * Enable/disable the fan.
 *
 * Should be called by whatever function enables the power supply to the fan.
 */
void fan_enable(int enable);

/**
 * Enable/disable fan RPM control logic.
 *
 * @param rpm_mode      Enable (1) or disable (0) RPM control loop; when
 *                      disabled, fan duty cycle will be used.
 */
void fan_set_rpm_mode(int enable);

/**
 * Get the current fan RPM.
 */
int fan_get_rpm(void);

/**
 * Get the target fan RPM.
 */
int fan_get_target_rpm(void);

/**
 * Set the target fan RPM.
 *
 * @param rpm   Target RPM; pass -1 to set fan to maximum.
 */
void fan_set_target_rpm(int rpm);

/**
 * Set the fan PWM duty cycle (0-100), disabling the automatic control.
 */
void fan_set_duty(int percent);


#endif
