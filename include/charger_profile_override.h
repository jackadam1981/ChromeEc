/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Charger profile override for fast charging
 */

#ifndef __CROS_EC_CHARGER_PROFILE_OVERRIDE_H
#define __CROS_EC_CHARGER_PROFILE_OVERRIDE_H

#include "charge_state_v2.h"

#define TEMPC_FLOAT_TO_INT(c) ((c) * 10)

/* Charge profile override info */
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

/* Charge profile override config */
struct fast_charge_config {
	/* Default temperature range */
	int temp_range_def;
	/* lower limit of voltage */
	int vtg_low_limit;
	/* higher limit of voltage */
	int vtg_high_limit;
};

enum fast_chg_voltage_ranges {
	VOLTAGE_RANGE_LOW,
	VOLTAGE_RANGE_HIGH,
};

struct fast_charge_params {
	const struct fast_charge_profile *chg_profile_info;
	const struct fast_charge_config *chg_config_info;
};

/* keep track of previuos charge profile info */
extern struct fast_charge_profile *prev_chg_profile_info;

/**
 * Optional customization of charger profile override for fast charging.
 *
 * On input, the struct reflects the default behavior. The function can make
 * changes to the state, requested_voltage, or requested_current.
 *
 * @param curr Charge state machine data.
 *
 * @return
 *   >0    Desired time in usec for this poll period.
 *   0     Use the default poll period (which varies with the state).
 *  <0     An error occurred. The poll time will be shorter than usual.
 *         Too many errors in a row may trigger some corrective action.
 */
int charger_profile_override(struct charge_state_data *curr);

/**
 * Common code of charger profile override for fast charging.
 *
 * @param curr Charge state machine data.
 * @param chg_profile_info Charge profile override info.
 * @param chg_config_info Charge profile override config.
 *
 * @return
 *   >0    Desired time in usec for this poll period.
 *   0     Use the default poll period (which varies with the state).
 *  <0     An error occurred. The poll time will be shorter than usual.
 *         Too many errors in a row may trigger some corrective action.
 */
int charger_profile_override_common(struct charge_state_data *curr,
			const struct fast_charge_profile *chg_profile_info,
			const struct fast_charge_config *chg_config_info);

/*
 * Access to custom profile params through host commands.
 * What this does is up to the implementation.
 */
enum ec_status charger_profile_override_get_param(uint32_t param,
						  uint32_t *value);
enum ec_status charger_profile_override_set_param(uint32_t param,
						  uint32_t value);

#endif /* __CROS_EC_CHARGER_PROFILE_OVERRIDE_H */
