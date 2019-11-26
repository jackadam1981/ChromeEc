/* Copyright 2019 The Chromium OS Authors. All rights reserved.
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
	int rpm[FAN_CH_COUNT];
};

static const struct fan_step *ptr;

static const struct fan_step fan_table_clamshell[] = {
	{
		/* level 0 */
		.on = {0, -1, 45, 54},
		.off = {99, -1, 0, 52},
		.rpm = {0, 0},
	},
	{
		/* level 1 */
		.on = {0, -1, 46, 56},
		.off = {99, -1, 45, 54},
		.rpm = {4200, 4700},
	},
	{
		/* level 2 */
		.on = {0, -1, 47, 58},
		.off = {99, -1, 46, 56},
		.rpm = {4400, 4900},
	},
	{
		/* level 3 */
		.on = {0, -1, 48, 60},
		.off = {99, -1, 47, 58},
		.rpm = {4600, 5100},
	},
	{
		/* level 4 */
		.on = {80, -1, 49, 62},
		.off = {74, -1, 48, 60},
		.rpm = {4800, 5300},
	},
	{
		/* level 5 */
		.on = {85, -1, 50, 64},
		.off = {79, -1, 49, 62},
		.rpm = {5200, 5700},
	},
	{
		/* level 6 */
		.on = {90, -1, 51, 66},
		.off = {84, -1, 50, 64},
		.rpm = {5600, 6100},
	},
	{
		/* level 7 */
		.on = {127, -1, 127, 127},
		.off = {89, -1, 51, 66},
		.rpm = {6000, 6500},
	},
};

static const struct fan_step fan_table_tablet[] = {
	{
		/* level 0 */
		.on = {0, -1, 42, 40},
		.off = {99, -1, 0, 0},
		.rpm = {0, 0},
	},
	{
		/* level 1 */
		.on = {0, -1, 43, 42},
		.off = {99, -1, 42, 37},
		.rpm = {0, 0},
	},
	{
		/* level 2 */
		.on = {0, -1, 44, 44},
		.off = {99, -1, 43, 39},
		.rpm = {0, 0},
	},
	{
		/* level 3 */
		.on = {0, -1, 45, 46},
		.off = {99, -1, 44, 41},
		.rpm = {0, 0},
	},
	{
		/* level 4 */
		.on = {80, -1, 46, 48},
		.off = {74, -1, 45, 43},
		.rpm = {4800, 5300},
	},
	{
		/* level 5 */
		.on = {85, -1, 47, 50},
		.off = {79, -1, 46, 45},
		.rpm = {5200, 5700},
	},
	{
		/* level 6 */
		.on = {90, -1, 60, 65},
		.off = {84, -1, 47, 47},
		.rpm = {5600, 6100},
	},
	{
		/* level 7 */
		.on = {127, -1, 127, 127},
		.off = {89, -1, 53, 57},
		.rpm = {6000, 6500},
	},
};

#define NUM_FAN_LEVELS ARRAY_SIZE(fan_table_clamshell)

BUILD_ASSERT(ARRAY_SIZE(fan_table_clamshell) ==
	ARRAY_SIZE(fan_table_tablet));

int fan_table_to_rpm(int fan, int *temp)
{
	static int current_level;
	static int prev_tmp[TEMP_SENSOR_COUNT];
	static int new_rpm;
	int i;

	if (tablet_get_mode())
		ptr = &fan_table_clamshell[0];
	else
		ptr = &fan_table_tablet[0];

	/*
	 * Compare the current and previous temperature, we have
	 * the three paths :
	 *  1. decreasing path. (check the release point)
	 *  2. increasing path. (check the trigger point)
	 *  3. invariant path. (return the current RPM)
	 */

	if (temp[TEMP_SENSOR_1] < prev_tmp[TEMP_SENSOR_1] ||
		temp[TEMP_SENSOR_3] < prev_tmp[TEMP_SENSOR_3] ||
		temp[TEMP_SENSOR_4] < prev_tmp[TEMP_SENSOR_4]) {
		for (i = current_level; i > 0; i--) {
			if ((temp[TEMP_SENSOR_1] < ptr[i].off[TEMP_SENSOR_1] &&
				 temp[TEMP_SENSOR_4] < ptr[i].off[TEMP_SENSOR_4]) &&
				 temp[TEMP_SENSOR_3] < ptr[i].off[TEMP_SENSOR_3])
				current_level = i - 1;
			else
				break;
		}
	} else if (temp[TEMP_SENSOR_1] > prev_tmp[TEMP_SENSOR_1] ||
		   temp[TEMP_SENSOR_3] > prev_tmp[TEMP_SENSOR_3] ||
		   temp[TEMP_SENSOR_4] > prev_tmp[TEMP_SENSOR_4]) {
		for (i = current_level; i < NUM_FAN_LEVELS; i++) {
			if ((temp[TEMP_SENSOR_1] > ptr[i].on[TEMP_SENSOR_1] &&
				 temp[TEMP_SENSOR_4] > ptr[i].on[TEMP_SENSOR_4]) ||
				 temp[TEMP_SENSOR_3] > ptr[i].on[TEMP_SENSOR_3])
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

	switch (fan) {
	case FAN_CH_0:
			new_rpm = ptr[current_level].rpm[FAN_CH_0];
		break;
	case FAN_CH_1:
			new_rpm = ptr[current_level].rpm[FAN_CH_1];
		break;
	default:
		break;
	}

	return new_rpm;
}

void board_override_fan_control(int *tmp)
{
	int new_rpm;
	int fan;

	new_rpm = 0;

	if (chipset_in_state(CHIPSET_STATE_ON |
		CHIPSET_STATE_ANY_SUSPEND)) {
		for (fan = 0; fan < fan_get_count(); fan++) {
			fan_set_rpm_mode(FAN_CH(fan), 1);
			new_rpm = fan_table_to_rpm(fan, (int *)tmp);
			fan_set_rpm_target(FAN_CH(fan), new_rpm);
		}
	}
}
