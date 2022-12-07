
/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charger.h"
#include "charge_state.h"
#include "console.h"
#include "extpower.h"
#include "temp_sensor/temp_sensor.h"
#include "util.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(smart_battery);

/*
 * Dynamic changing charge current.
 */

struct chg_curr_step {
	int on;         /* on temperature (Degree C) */
	int off;	/* off temperature (Degree C) */
	int curr;	/* charge current (mA) */
};

static const struct chg_curr_step chg_curr_table[] = {
	{.on = 0, .off =  0, .curr = __INT32_MAX__ },	/* normal charge */
	{.on = 56, .off = 50, .curr = 2000 },
};
/*
static const struct chg_curr_step chg_curr_table[] = {
	{.on = 45, .off =  0, .curr = __INT32_MAX__ },
	{.on = 50, .off = 45, .curr = 3000 },
	{.on = 56, .off = 50, .curr = 2000 },

};
*/
#define NUM_CHG_CURRENT_LEVELS ARRAY_SIZE(chg_curr_table)

#define BOARD_TEMP_TEST

#ifdef BOARD_TEMP_TEST
static int manual_temp = -1;
#endif

__override int board_charger_profile_override(
				struct charge_state_data *curr)
{
	static int current_level;
	static int previous_level = NUM_CHG_CURRENT_LEVELS;
	static int current_temp, previous_temp;
	int i, rv;

	if (curr->state != ST_CHARGE)
		return EC_SUCCESS;

	rv = temp_sensor_read(TEMP_SENSOR_ID_BY_DEV(
		DT_NODELABEL(temp_charger)),
		&current_temp);

	if (rv != EC_SUCCESS)
		return rv;

	current_temp = K_TO_C(current_temp);

#ifdef BOARD_TEMP_TEST
	if (manual_temp != -1)
		current_temp = manual_temp;
	LOG_WRN("chg_temp_c: %d", current_temp);
#endif

	if (current_temp < previous_temp) {
		for (i = current_level; i >= 0; i--) {
			if (current_temp <= chg_curr_table[i].off)
				current_level = i - 1;
			else
				break;
		}
	} else if (current_temp > previous_temp) {
		for (i = current_level + 1; i < NUM_CHG_CURRENT_LEVELS; i++) {
			if (current_temp >= chg_curr_table[i].on)
				current_level = i;
			else
				break;
		}
	}

	if (current_level < 0)
		current_level = 0;

	if (current_level != previous_level)
		LOG_WRN("Setting charger limit to %d mA",
			chg_curr_table[current_level].curr);

	previous_temp = current_temp;
	previous_level = current_level;

#ifdef BOARD_TEMP_TEST
	LOG_WRN("level: %d, batt_current: %d, limit_current: %d",
				current_level,
				curr->requested_current,
				chg_curr_table[current_level].curr);
#endif

	curr->requested_current = MIN(curr->requested_current,
				chg_curr_table[current_level].curr);

	return EC_SUCCESS;
}

#ifdef BOARD_TEMP_TEST
static int command_temp_test(int argc, const char **argv)
{
	char *e;
	int t;

	if (argc > 1) {
		t = strtoi(argv[1], &e, 0);

		if (*e) {
			LOG_WRN("Invalid test temp");
			return EC_ERROR_INVAL;
		}

		manual_temp = t;
		LOG_WRN("manual temp is %d", manual_temp);

		return EC_SUCCESS;
	}
	manual_temp = -1;
	LOG_WRN("manual temp reset");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(tt, command_temp_test, "[temperature]",
			"set manual temperature for test");
#endif
