/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "extpower.h"
#include "hooks.h"
#include "power.h"
#include "temp_sensor/temp_sensor.h"
#include "usb_pd.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board_thermal, LOG_LEVEL_INF);

#define LIMIT_NONE 9999

static int current_limit;

struct temp_step {
	int on;
	int off;
	int ilimi;
};

static const struct temp_step typec_ilim_table[] = {
	{ .on = 0, .off = 0, .ilimi = LIMIT_NONE },
	{ .on = 59, .off = 53, .ilimi = 1500 },
};

#define NUM_TYPEC_ILIM_LEVELS ARRAY_SIZE(typec_ilim_table)

static int typec_ilim_level(int prev_level)
{
	int rv;
	int chg_temp_c;
	int thermal_sensor0;
	int lvl;
	static int prev_temp;

	rv = temp_sensor_read(TEMP_SENSOR_ID_BY_DEV(DT_NODELABEL(temp_charger)),
			      &thermal_sensor0);
	chg_temp_c = K_TO_C(thermal_sensor0);

	if (rv != EC_SUCCESS)
		return prev_level;

	if (chg_temp_c < prev_temp &&
	    chg_temp_c <= typec_ilim_table[prev_level].off) {
		lvl = prev_level - 1;
		/* Prevent level always minus 0 */
		if (lvl < 0)
			lvl = 0;
	} else if (chg_temp_c > prev_temp &&
		   chg_temp_c >= typec_ilim_table[prev_level + 1].on) {
		lvl = prev_level + 1;
		/* Prevent level always over table steps */
		if (lvl >= NUM_TYPEC_ILIM_LEVELS)
			lvl = NUM_TYPEC_ILIM_LEVELS - 1;
	}
	prev_temp = chg_temp_c;

	return lvl;
}

static void typec_ilim_control(void)
{
	int i;
	bool any_port_is_source = false;
	int current_lvl;
	static int prev_lvl;

	for (i = 0; i < board_get_usb_pd_port_count(); i++) {
		if (pd_get_power_role(i) == PD_ROLE_SOURCE) {
			any_port_is_source = true;
		}
	}

	if (!any_port_is_source) {
		current_limit = LIMIT_NONE;
		return;
	}
	current_lvl = typec_ilim_level(prev_lvl);

	if (current_lvl != prev_lvl) {
		current_limit = typec_ilim_table[current_lvl].ilimi;
		prev_lvl = current_lvl;

		LOG_INF("Thermal detect, ilimit=%d mA", current_limit);
	}
}
DECLARE_HOOK(HOOK_SECOND, typec_ilim_control, HOOK_PRIO_TEMP_SENSOR_DONE);

int charger_profile_override(struct charge_state_data *curr)
{
	curr->requested_current = MIN(curr->requested_current, current_limit);

	return EC_SUCCESS;
}

enum ec_status charger_profile_override_get_param(uint32_t param,
						  uint32_t *value)
{
	return EC_RES_INVALID_PARAM;
}

enum ec_status charger_profile_override_set_param(uint32_t param,
						  uint32_t value)
{
	return EC_RES_INVALID_PARAM;
}
