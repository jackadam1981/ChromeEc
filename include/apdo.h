/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_APDO_H
#define __CROS_EC_APDO_H

#include <stdbool.h>
#include <stdint.h>

#include "common.h"
#include "charge_state_v2.h"

/*
 * Get adpative voltage in the current system load
 *
 * @param port: the evaluated port
 *
 * @return a voltage that the adapter supports to charge at the given port.
 */
int apdo_get_adaptive_voltage(int port);

/*
 * Get the input power of the given port.
 *
 * input_power = battery_desired_power + system_desired_power
 *
 * @param port: The given port
 * @param vbus: VBUS in mV
 * @param input_curr: input current in mA
 *
 * @return input_power if the result of vbus * input_curr in mW
 */
int apdo_get_desired_input_power(int port, int *vbus, int *input_curr);

/*
 * Check if APDO is enabled.
 *
 * @return true if enabled, false otherwise.
 */
bool apdo_is_enabled(void);

/*
 * Enable/Disable APDO
 *
 * @param en: enable/disable
 */
void apdo_enable(bool en);

/*
 * Evaluate the system power and update it if a new PD power request is
 * needed at the given port.
 *
 * @param port: the port to be evluated.
 * @return true if a new power request, or false otherwise.
 */
bool apdo_has_new_power_request(int port);

/*
 * APDO initialization.
 *
 * @param port: the port to be initialized.
 */
void apdo_init(int port);

/*
 * Resets the power evaluation timer. This should be called if a new
 * power request is negotiated.
 *
 * @param port: the port for timer reset.
 */
void apdo_reset_stable(int port);
#endif /* __CROS_EC_APDO_H */
