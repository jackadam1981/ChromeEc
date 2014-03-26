/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charger.h"
#include "timer.h"

#ifndef __CROS_EC_CHARGE_STATE_V2_H
#define __CROS_EC_CHARGE_STATE_V2_H

/*
 * The values exported by charge_get_state() and charge_get_flags() are used
 * only to control the LEDs (with one not-quite-correct exception). For V2
 * we use a different set of states internally.
 */
enum charge_state_v2 {
	ST_IDLE = 0,
	ST_DISCHARGE,
	ST_CHARGE,
	ST_PRECHARGE,

	NUM_STATES_V2
};

/* TODO(HEY): this should be part of charger.h */
struct charger_params {
	const struct charger_info *info;	/* charger IC limits */
	int charging_voltage;
	int charging_current;
	int input_current;			/* max input current allowed */
};

struct charge_state_data {
	timestamp_t ts;
	int ac;
	struct charger_params chg;
	enum battery_present batt_is_present;
	struct batt_params batt;
	int batt_remaining_capacity;
	enum charge_state_v2 state;
	/* What are we asking the charger for? */
	int requested_voltage;
	int requested_current;
};

/* Optional customization */
void charger_profile_override(struct charge_state_data *);
/*
 * HEY: We should provide a way to read the current setting, too. At least for
 * the LEDs or lightbar, but the AP might want to know too.
 */
int charger_profile_override_enable(int enable);


#endif /* __CROS_EC_CHARGE_STATE_V2_H */

