/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "battery.h"
#include "charge_state.h"
#include "charger.h"
#include "console.h"
#include "extpower.h"
#include "temp_sensor/temp_sensor.h"
#include "util.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(smart_battery);

/*
 * Dynamic changing charge current.
 */

struct temp_chg_step {
	int low; /* temp thershold ('C) to lower level*/
	int high; /* temp thershold ('C) to higher level */
	int current; /* charging limitation (mA) */
};

static const struct temp_chg_step temp_chg_table[] = {
	{ .low = 0, .high = 56, .current = __INT32_MAX__ },
	{ .low = 50, .high = 100, .current = 2000 },
};
#define NUM_TEMP_CHG_LEVELS ARRAY_SIZE(temp_chg_table)


__override int board_charger_profile_override(struct charge_state_data *curr)
{
	static int current_level;
	int charger_temp, charger_temp_c;

	if (curr->state != ST_CHARGE)
		return 0;

	temp_sensor_read(TEMP_SENSOR_ID_BY_DEV(DT_NODELABEL(temp_charger)),
			 &charger_temp);

	charger_temp_c = K_TO_C(charger_temp);

	if (charger_temp_c <= temp_chg_table[current_level].low)
		current_level--;
	else if (charger_temp_c >= temp_chg_table[current_level].high)
		current_level++;

	if (current_level < 0)
		current_level = 0;

	if (current_level >= NUM_TEMP_CHG_LEVELS)
		current_level = NUM_TEMP_CHG_LEVELS - 1;

	curr->requested_current = MIN(curr->requested_current,
				      temp_chg_table[current_level].current);

	return EC_SUCCESS;
}
