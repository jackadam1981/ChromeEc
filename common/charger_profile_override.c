/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Charger profile override for fast charging
 */

#include "charger_profile_override.h"
#include "console.h"
#include "ec_commands.h"

#ifdef CONFIG_CMD_CHARGER_PROFILE_OVERRIDE_TEST
static int fast_charge_test_on;
static int test_temp_c;
static int test_vtg_mV;
#endif

static int fast_charging_allowed = 1;

int charger_profile_override_common(struct charge_state_data *curr,
			const struct fast_charge_profile *chg_profile_info,
			const struct fast_charge_config *chg_config_info,
			int batt_vtg_max)
{
	/* temp in 0.1 deg C */
	int temp_c = curr->batt.temperature - 2731;
	/* keep track of previuos voltage range */
	static enum fast_chg_voltage_ranges voltage_range = VOLTAGE_RANGE_LOW;
	/* Current and previous battery voltage */
	int batt_voltage;
	static int prev_batt_voltage;

#ifdef CONFIG_CMD_CHARGER_PROFILE_OVERRIDE_TEST
	temp_c = TEMPC_FLOAT_TO_INT(test_temp_c);
	curr->batt.voltage = test_vtg_mV;
#endif

	/*
	 * Determine temperature range.
	 * If temp reading was bad, use last range.
	 */
	if (!(curr->batt.flags & BATT_FLAG_BAD_TEMPERATURE)) {
		while (chg_profile_info) {
			if (temp_c <= chg_profile_info->temp_c) {
				prev_chg_profile_info =
				(struct fast_charge_profile *) chg_profile_info;
				break;
			}
			chg_profile_info++;
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
				prev_chg_profile_info->curr_high_vtg :
				prev_chg_profile_info->curr_low_vtg;
	curr->requested_voltage = curr->requested_current ? batt_vtg_max : 0;

#ifdef CONFIG_CMD_CHARGER_PROFILE_OVERRIDE_TEST
	if (fast_charge_test_on)
		ccprintf("Fast charge profile i=%dmA, v=%dmV\n",
			curr->requested_current, curr->requested_voltage);
#endif

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

/*
 * Manipulate the temperature and voltage values and check if the correct
 * fast charging profile is selected.
 */
#ifdef CONFIG_CMD_CHARGER_PROFILE_OVERRIDE_TEST
static int command_fastcharge_test(int argc, char **argv)
{
	char *e;

	if (!parse_bool(argv[1], &fast_charge_test_on))
		return EC_ERROR_PARAM1;

	if (!fast_charge_test_on) {
		if (argc != 2)
			return EC_ERROR_PARAM_COUNT;
		else
			return EC_SUCCESS;
	}

	if (fast_charge_test_on && argc != 4)
		return EC_ERROR_PARAM_COUNT;

	test_temp_c = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	test_vtg_mV = strtoi(argv[3], &e, 0);
	if (*e || test_vtg_mV < 0)
		return EC_ERROR_PARAM3;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(fastchgtest, command_fastcharge_test,
			"off | on temp_c vtg_mV",
			"Check if fastcharge profile works");
#endif
