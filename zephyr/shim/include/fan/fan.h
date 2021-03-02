/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_SHIM_INCLUDE_FAN_FAN_H_
#define ZEPHYR_SHIM_INCLUDE_FAN_FAN_H_

/**
 * Get fan rpm value
 *
 * @param   ch      operation channel
 * @return          Actual rpm
 */
int fan_rpm(int ch);

/**
 * Setup hardware for rpm measurement
 *
 * @param   ch      operation channel
 * @param   flags   configure fan flags
 */
void fan_rpm_setup(int ch, unsigned int flags);

/**
 * Get pwm id for the operation channel
 *
 * @param   ch      operation channel
 * @return          pwm channel
 */
enum pwm_channel fan_get_pwm_id(int ch);

#endif /* ZEPHYR_SHIM_INCLUDE_FAN_FAN_H_ */
