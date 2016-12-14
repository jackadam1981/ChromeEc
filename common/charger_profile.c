/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Charger profile for fast charging
 */

#include "charger_profile.h"
#include "console.h"
#include "ec_commands.h"
#include "util.h"

/* keep track of last temperature range */
#define TEMP_RANGE chg_config_info->temp_last_range

static int fast_charging_allowed = 1;

int charger_profile_override_common(struct charge_state_data *curr,
			const struct fast_charge_profile *chg_profile_info[],
			struct fast_charge_config *chg_config_info)
{
	int i;
	/* temp in 0.1 deg C */
	int temp_c = curr->batt.temperature - 2731;
	/* keep track of last voltage range */
	static enum fast_chg_voltage_ranges voltage_range = VOLTAGE_RANGE_LOW;
	/* Current and previous battery voltage */
	int batt_voltage;
	static int prev_batt_voltage;

	/*
	 * Determine temperature range.
	 * If temp reading was bad, use last range.
	 */
	if (!(curr->batt.flags & BATT_FLAG_BAD_TEMPERATURE)) {
		for (i = 0; i < chg_config_info->temp_ranges; i++) {
			if (temp_c <= chg_profile_info[i]->temp_c) {
				TEMP_RANGE = i;
				break;
			}
		}
	}

	/* If battery voltage reading is bad, use the last reading. */
	if (curr->batt.flags & BATT_FLAG_BAD_VOLTAGE) {
		batt_voltage = prev_batt_voltage;
	} else {
		batt_voltage = prev_batt_voltage = curr->batt.voltage;
		if (batt_voltage < chg_config_info->vtg_low_limit)
			voltage_range = VOLTAGE_RANGE_LOW;
		else if (batt_voltage > chg_config_info->vtg_high_limit)
			voltage_range = VOLTAGE_RANGE_HIGH;
	}

	/*
	 * If we are not charging or we aren't using fast charging profiles,
	 * then do not override desired current and voltage.
	 */
	if (curr->state != ST_CHARGE || !fast_charging_allowed)
		return 0;
	/*
	 * Okay, impose our custom will:
	 */
	curr->requested_current = (voltage_range == VOLTAGE_RANGE_HIGH) ?
				chg_profile_info[TEMP_RANGE]->curr_high_vtg :
				chg_profile_info[TEMP_RANGE]->curr_low_vtg;
	curr->requested_voltage = chg_profile_info[TEMP_RANGE]->voltage;

	return 0;
}

/* Customs options controllable by host command. */
#define PARAM_FASTCHARGE (CS_PARAM_CUSTOM_PROFILE_MIN + 0)

enum ec_status charger_profile_override_get_param(uint32_t param,
						  uint32_t *value)
{
	if (param == PARAM_FASTCHARGE) {
		*value = fast_charging_allowed;
		return EC_RES_SUCCESS;
	}
	return EC_RES_INVALID_PARAM;
}

enum ec_status charger_profile_override_set_param(uint32_t param,
						  uint32_t value)
{
	if (param == PARAM_FASTCHARGE) {
		fast_charging_allowed = value;
		return EC_RES_SUCCESS;
	}
	return EC_RES_INVALID_PARAM;
}

#ifdef CONFIG_CMD_CHARGER_PROFILE_OVERRIDE
static int command_fastcharge(int argc, char **argv)
{
	if (argc > 1 && !parse_bool(argv[1], &fast_charging_allowed))
		return EC_ERROR_PARAM1;

	ccprintf("fastcharge %s\n", fast_charging_allowed ? "on" : "off");

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(fastcharge, command_fastcharge,
			"[on|off]",
			"Get or set fast charging profile");
#endif
