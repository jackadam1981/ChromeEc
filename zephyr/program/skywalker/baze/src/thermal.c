/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/ucsi_v3.h"
#include "hooks.h"
#include "temp_sensor/temp_sensor.h"
#include "usbc/pdc_dpm.h"
#include "usbc/pdc_power_mgmt.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board_thermal, LOG_LEVEL_INF);

struct temp_step {
	int on;
	int off;
	enum usb_typec_current_t ilimi;
};

static const struct temp_step typec_ilim_table[] = {
	{ .on = 0, .off = 0, .ilimi = TC_CURRENT_3_0A },
	{ .on = 59, .off = 53, .ilimi = TC_CURRENT_1_5A },
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
	int max_port;
	static int prev_lvl;

	for (i = 0; i < board_get_usb_pd_port_count(); i++) {
		if (pd_get_power_role(i) == PD_ROLE_SOURCE) {
			any_port_is_source = true;
			if (pdc_dpm_get_source_current(i) == 3000) {
				max_port = i;
				break;
			}
		}
	}

	if (!any_port_is_source)
		return;

	current_lvl = typec_ilim_level(prev_lvl);

	if (current_lvl != prev_lvl) {
		enum usb_typec_current_t rp =
			typec_ilim_table[current_lvl].ilimi;
		pdc_power_mgmt_set_current_limit(max_port, rp);
		prev_lvl = current_lvl;

		LOG_INF("Thermal detect, ilimit=%d mA", rp);
	}
}
DECLARE_HOOK(HOOK_SECOND, typec_ilim_control, HOOK_PRIO_TEMP_SENSOR_DONE);
