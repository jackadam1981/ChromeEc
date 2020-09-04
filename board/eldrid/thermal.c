/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "fan.h"
#include "hooks.h"
#include "host_command.h"
#include "tablet_mode.h"
#include "temp_sensor.h"
#include "thermal.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_THERMAL, outstr)
#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ## args)

struct fan_step {
	/*
	 * Sensor 1~4 trigger point, set -1 if we're not using this
	 * sensor to determine fan speed.
	 */
	int8_t on[TEMP_SENSOR_COUNT];

	/*
	 * Sensor 1~4 trigger point, set -1 if we're not using this
	 * sensor to determine fan speed.
	 */
	int8_t off[TEMP_SENSOR_COUNT];

	/* Fan 1~2 rpm */
	uint16_t rpm[FAN_CH_COUNT];
};

static const struct fan_step *fan_step_table;

static const struct fan_step fan_table_clamshell[] = {
	{
		/* level 0 */
		.on = {36, -1, -1, -1},
		.off = {0, -1, -1, -1},
		.rpm = {0},
	},
	{
		/* level 1 */
		.on = {38, -1, -1, -1},
		.off = {36, -1, -1, -1},
		.rpm = {2000},
	},
	{
		/* level 2 */
		.on = {41, -1, -1, -1},
		.off = {39, -1, -1, -1},
		.rpm = {2600},
	},
	{
		/* level 3 */
		.on = {44, -1, -1, -1},
		.off = {42, -1, -1, -1},
		.rpm = {3000},
	},
	{
		/* level 4 */
		.on = {46, -1, -1, -1},
		.off = {44, -1, -1, -1},
		.rpm = {3300},
	},
	{
		/* level 5 */
		.on = {49, -1, -1, -1},
		.off = {47, -1, -1, -1},
		.rpm = {3600},
	},
	{
		/* level 6 */
		.on = {51, -1, -1, -1},
		.off = {49, -1, -1, -1},
		.rpm = {4200},
	},
	{
		/* level 7 */
		.on = {55, -1, -1, -1},
		.off = {52, -1, -1, -1},
		.rpm = {4700},
	},
};

#define NUM_FAN_LEVELS ARRAY_SIZE(fan_table_clamshell)

int fan_table_to_rpm(int fan, int *temp)
{
	static int current_level;
	static int prev_tmp[TEMP_SENSOR_COUNT];
	static int new_rpm;
	int i;

	//if (tablet_get_mode())
		//fan_step_table = fan_table_tablet;
//	else
	fan_step_table = fan_table_clamshell;

	/*
	 * Compare the current and previous temperature, we have
	 * the three paths :
	 *  1. decreasing path. (check the release point)
	 *  2. increasing path. (check the trigger point)
	 *  3. invariant path. (return the current RPM)
	 */
	 CPRINTS("temp: %d, prev_tmp: %d \n", temp[TEMP_SENSOR_1_CHARGER], prev_tmp[TEMP_SENSOR_1_CHARGER] );
	if (temp[TEMP_SENSOR_1_CHARGER] < prev_tmp[TEMP_SENSOR_1_CHARGER]) {
		for (i = current_level; i > 0; i--) {
			if (temp[TEMP_SENSOR_1_CHARGER] < fan_step_table[i].off[TEMP_SENSOR_1_CHARGER])
				current_level = i - 1;
			else
				break;
		}
	} else if (temp[TEMP_SENSOR_1_CHARGER] > prev_tmp[TEMP_SENSOR_1_CHARGER]) {
		for (i = current_level; i < NUM_FAN_LEVELS; i++) {
			if (temp[TEMP_SENSOR_1_CHARGER] > fan_step_table[i].on[TEMP_SENSOR_1_CHARGER])
				current_level = i + 1;
			else
				break;
		}
	}

	if (current_level < 0)
		current_level = 0;

	for (i = 0; i < TEMP_SENSOR_COUNT; ++i)
		prev_tmp[i] = temp[i];

	ASSERT(current_level < NUM_FAN_LEVELS);
	
	CPRINTS("current_level: %d\n", current_level);
	
	switch (fan) {
	case FAN_CH_0:
		new_rpm = fan_step_table[current_level].rpm[FAN_CH_0];
		break;
	default:
		break;
	}

	return new_rpm;
}

void board_override_fan_control(int fan, int *tmp)
{
	if (chipset_in_state(CHIPSET_STATE_ON |
		CHIPSET_STATE_ANY_SUSPEND)) {
		fan_set_rpm_mode(FAN_CH(fan), 1);
		fan_set_rpm_target(FAN_CH(fan),
			fan_table_to_rpm(fan, tmp));
	}
}
