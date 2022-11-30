/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "temp_sensor/temp_sensor.h"
#include "host_command.h"
#include "thermal.h"
#include "lid_switch.h"
#include "body_detection.h"

#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_THERMAL, format, ##args)

#define TEMP_AMB TEMP_SENSOR_ID(DT_NODELABEL(temp_sensor_amb))

static int last_amb_temp = -1;

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

/* Thermal table for DESKTOP_LID_OPEN_MODE, DESKTOP_LID_CLOSE_MODE, LAPTOP_MODE
 */
static const struct thermal_setting thermal_table[] = {
	{ .warn = 50, .release_warn = 45, .fan_off = 31, .fan_max = 38 },
	{ .warn = 55, .release_warn = 50, .fan_off = 32, .fan_max = 39 },
	{ .warn = 45, .release_warn = 40, .fan_off = 30, .fan_max = 38 },
};

/* Change temp sensor amb-pct2075's config setting */
static void thermal_threshold_set(int mode)
{
	thermal_params[TEMP_AMB].temp_host[EC_TEMP_THRESH_WARN] =
		C_TO_K(thermal_table[mode].warn);
	thermal_params[TEMP_AMB].temp_host_release[EC_TEMP_THRESH_WARN] =
		C_TO_K(thermal_table[mode].release_warn);
	thermal_params[TEMP_AMB].temp_fan_off =
		C_TO_K(thermal_table[mode].fan_off);
	thermal_params[TEMP_AMB].temp_fan_max =
		C_TO_K(thermal_table[mode].fan_max);
}

/* Switch thermal table when mode change */
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

/* Set SCI event to host for temperature change */
static void detect_temp_change(void)
{
	int t, rv;

	rv = temp_sensor_read(TEMP_AMB, &t);
	switch (rv) {
	case EC_SUCCESS:
		if (last_amb_temp != t) {
			last_amb_temp = t;
			host_set_single_event(EC_HOST_EVENT_THERMAL_THRESHOLD);
		}
		break;
	case EC_ERROR_NOT_POWERED:
		CPRINTS("Temp sensor: Not powered");
		break;
	case EC_ERROR_INVAL:
		CPRINTS("Temp sensor: Invalid id");
		break;
	default:
		CPRINTS("Temp sensor: Error %d", rv);
		break;
	}
}
DECLARE_HOOK(HOOK_SECOND, detect_temp_change, HOOK_PRIO_TEMP_SENSOR_DONE);
