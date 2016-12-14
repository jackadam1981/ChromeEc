/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Charger profile for fast charging
 */

#ifndef __CROS_EC_CHARGER_PROFILE_H
#define __CROS_EC_CHARGER_PROFILE_H

#include "charge_state_v2.h"

#define TEMPC_FLOAT_TO_INT(c) ((c) * 10)

struct fast_charge_profile {
	/* temperature in C */
	int temp_c;
	/* charge current at high voltage */
	int curr_high_vtg;
	/* charge current at low voltage */
	int curr_low_vtg;
	/* charge voltage */
	int voltage;
};

struct fast_charge_config {
	/* total number of temperature ranges */
	int temp_ranges;
	/* keep track of last temperature range */
	int temp_last_range;
	/* lower limit of voltage */
	int vtg_low_limit;
	/* higher limit of voltage */
	int vtg_high_limit;
};

enum fast_chg_voltage_ranges {
	VOLTAGE_RANGE_LOW,
	VOLTAGE_RANGE_HIGH,
};

/*
 * Optional customization.
 *
 * On input, the struct reflects the default behavior. The function can make
 * changes to the state, requested_voltage, or requested_current.
 *
 * Return value:
 *   >0    Desired time in usec for this poll period.
 *   0     Use the default poll period (which varies with the state).
 *  <0     An error occurred. The poll time will be shorter than usual. Too
 *           many errors in a row may trigger some corrective action.
 */
int charger_profile_override(struct charge_state_data *);

int charger_profile_override_common(struct charge_state_data *curr,
			const struct fast_charge_profile *chg_profile_info[],
			struct fast_charge_config *chg_config_info);


/*
 * Access to custom profile params through host commands.
 * What this does is up to the implementation.
 */
enum ec_status charger_profile_override_get_param(uint32_t param,
						  uint32_t *value);
enum ec_status charger_profile_override_set_param(uint32_t param,
						  uint32_t value);

#endif /* __CROS_EC_CHARGER_PROFILE_H */
