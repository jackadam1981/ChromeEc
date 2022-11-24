/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "thermal.h"
#include "lid_switch.h"
#include "body_detection.h"

#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_THERMAL, format, ##args)

#define TEMP_AMB_ID DT_NODE_CHILD_IDX(DT_PATH(named_temp_sensors, amb_pct2075))

enum dynamic_thermal_switch_mode {
	DESKTOP_LID_OPEN_MODE,
	DESKTOP_LID_CLOSE_MODE,
	LAPTOP_MODE
};

struct thermal_setting {
	int warn;
	int release_warn;
	int fan_off;
	int fan_max;
};

/*Thermal table for DESKTOP_LID_OPEN_MODE, DESKTOP_LID_CLOSE_MODE, LAPTOP_MODE*/
static const struct thermal_setting thermal_table[] = {
	{ .warn = 50, .release_warn = 45, .fan_off = 35, .fan_max = 40 },
	{ .warn = 55, .release_warn = 50, .fan_off = 38, .fan_max = 43 },
	{ .warn = 45, .release_warn = 40, .fan_off = 33, .fan_max = 42 },
};

/*Change temp sensor amb-pct2075's config setting*/
static void thermal_threshold_set(int mode)
{
	thermal_params[TEMP_AMB_ID].temp_host[EC_TEMP_THRESH_WARN] =
		C_TO_K(thermal_table[mode].warn);
	thermal_params[TEMP_AMB_ID].temp_host_release[EC_TEMP_THRESH_WARN] =
		C_TO_K(thermal_table[mode].release_warn);
	thermal_params[TEMP_AMB_ID].temp_fan_off =
		C_TO_K(thermal_table[mode].fan_off);
	thermal_params[TEMP_AMB_ID].temp_fan_max =
		C_TO_K(thermal_table[mode].fan_max);
}

/*Switch thermal when mode change*/
static void thermal_table_switch(void)
{
	enum body_detect_states body_state = body_detect_get_state();

	if (body_state == BODY_DETECTION_OFF_BODY) {
		if (lid_is_open()) {
			thermal_threshold_set(DESKTOP_LID_OPEN_MODE);
			CPRINTS("Thermal: Desktop lid open mode");
		} else {
			thermal_threshold_set(DESKTOP_LID_CLOSE_MODE);
			CPRINTS("Thermal: Desktop lid close mode");
		}
	} else {
		thermal_threshold_set(LAPTOP_MODE);
		CPRINTS("Thermal: Laptop mode");
	}
}
DECLARE_HOOK(HOOK_INIT, thermal_table_switch, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_LID_CHANGE, thermal_table_switch, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_BODY_DETECT_CHANGE, thermal_table_switch, HOOK_PRIO_DEFAULT);
