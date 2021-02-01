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
 * Get PDO index regarding to the input power.
 *
 * TODO
 * @return the PDO index or -1 if unfit.
 * TODO
 */
int apdo_find_pdo_index(int port, uint32_t src_cap_cnt,
			const uint32_t *const src_caps, int max_mv,
			uint32_t *selected_pdo);

int apdo_get_adaptive_voltage(int port);
/*
 * Get the input power of the given port.
 *
 * input_power = battery_desired_power + system_desired_power
 *
 * @param port: The given port
 * @param input_power: The calculated input power
 *
 * @return input_power if success, or 0 otherwise
 */
int apdo_get_desired_input_power(int port, int *vbus, int *input_curr);

/*
 * Records the requested PD voltage
 *
 * @param port: The port
 * @supply_voltage: Requested voltage in mV
 */
void apdo_set_request_power(int port, int supply_voltage, int curr_limit);

/*
 * Check if APDO is enabled.
 *
 * @return true if enabled, false otherwise.
 */
bool apdo_is_enabled(void);

/*
 * Enable/Diable APDO
 *
 * @param en: enable/disable
 */
void apdo_enable(bool en);

/*
 * Issue a new PD power request at the current charging port if needed.
 */
bool apdo_try_new_power_request(int port);
bool apdo_has_new_power_request(int port);

/*
 * APDO initialization.
 */
void apdo_init(int port);
void apdo_reset(int port);


void apdo_reset_timer(void);
#endif /* __CROS_EC_APDO_H */
