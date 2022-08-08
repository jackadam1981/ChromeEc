/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Shotzo has a charger but has no battery.
 * Add fake battery subfunctions for charger functional requirements.
 * Add a fake battery info for charger to set Vsys.
 */

#include "charge_state.h"
#include "console.h"

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

static const struct battery_info info = {
	.voltage_max = 12500, /* mV */
	.voltage_normal = 12500,
	.voltage_min = 12500,
};

const struct battery_info *battery_get_info(void)
{
	return &info;
}

enum battery_disconnect_state battery_get_disconnect_state(void)
{
	return BATTERY_DISCONNECTED;
}

void battery_get_params(struct batt_params *batt)
{
	batt->flags = 0;
	batt->flags |= BATT_FLAG_BAD_TEMPERATURE;
	batt->flags |= BATT_FLAG_BAD_STATE_OF_CHARGE;
	batt->flags |= BATT_FLAG_BAD_VOLTAGE;
	batt->flags |= BATT_FLAG_BAD_CURRENT;
	batt->flags |= BATT_FLAG_BAD_REMAINING_CAPACITY;
	batt->flags |= BATT_FLAG_BAD_FULL_CAPACITY;
	batt->flags |= BATT_FLAG_BAD_STATUS;
	batt->flags |= BATT_FLAG_BAD_ANY;

	batt->desired_voltage = batt->desired_current = 0;
	batt->is_present = BP_NO;
}

void battery_validate_params(struct batt_params *batt)
{
	batt->flags |= BATT_FLAG_BAD_TEMPERATURE;
	batt->flags |= BATT_FLAG_BAD_STATE_OF_CHARGE;
}

int battery_time_to_empty(int *minutes)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int battery_time_to_full(int *minutes)
{
	return EC_ERROR_UNIMPLEMENTED;
}

void print_battery_debug(void)
{
	CPRINTS("Shotzo has no battery.");
}

int battery_is_cut_off(void)
{
	return 0; /* Always return NOT cut off */
}

int update_static_battery_info(void)
{
	return 0;
}

void update_dynamic_battery_info(void)
{
	/* Nothing to do.  Shotzo has no battery. */
}
