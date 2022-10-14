/* Copyright 2022 The ChromiumOS Authors.
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
#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)

enum temp_sensor_id {
	TEMP_SENSOR_1_CPU,
	TEMP_SENSOR_2_5V,
	TEMP_SENSOR_3_CHARGER,
	TEMP_SENSOR_COUNT
};

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
	/* Fan rpm */
	uint16_t rpm[FAN_CH_COUNT];
};

static const struct fan_step fan_step_table[] = {
	{
		/* level 0 */
		.on = { 44, 47, 0 },
		.off = { 99, 99, 99 },
		.rpm = { 0 },
	},
	{
		/* level 1 */
		.on = { 48, 48, 0 },
		.off = { 43, 45, 99 },
		.rpm = { 2600 },
	},
	{
		/* level 2 */
		.on = { 50, 49, 0 },
		.off = { 47, 46, 99 },
		.rpm = { 2800 },
	},
	{
		/* level 3 */
		.on = { 52, 50, 54 },
		.off = { 49, 47, 51 },
		.rpm = { 3000 },
	},
	{
		/* level 4 */
		.on = { 54, 56, 60 },
		.off = { 51, 48, 52 },
		.rpm = { 3300 },
	},
	{
		/* level 5 */
		.on = { 60, 60, 64 },
		.off = { 53, 52, 56 },
		.rpm = { 3600 },
	},
	{
		/* level 5 */
		.on = { 100, 100, 100 },
		.off = { 59, 54, 58 },
		.rpm = { 4100 },
	},
};
const int num_fan_levels = ARRAY_SIZE(fan_step_table);

int fan_table_to_rpm(int fan, int *temp)
{
	/* current fan level */
	static int current_level;

	/* previous sensor temperature */
	static int prev_tmp[TEMP_SENSOR_COUNT];
	int i;
	int new_rpm = 0;

	/*
	 * Compare the current and previous temperature, we have
	 * the three paths :
	 *  1. decreasing path. (check the release point)
	 *  2. increasing path. (check the trigger point)
	 *  3. invariant path. (return the current RPM)
	 */
	if (temp[TEMP_SENSOR_1_CPU] < prev_tmp[TEMP_SENSOR_1_CPU] ||
	    temp[TEMP_SENSOR_2_5V] < prev_tmp[TEMP_SENSOR_2_5V] ||
	    temp[TEMP_SENSOR_3_CHARGER] < prev_tmp[TEMP_SENSOR_3_CHARGER]) {
		for (i = current_level; i > 0; i--) {
			if (temp[TEMP_SENSOR_1_CPU] <
				    fan_step_table[i].off[TEMP_SENSOR_1_CPU] &&
			    temp[TEMP_SENSOR_2_5V] <
				    fan_step_table[i].off[TEMP_SENSOR_2_5V] &&
			    temp[TEMP_SENSOR_3_CHARGER] <
				    fan_step_table[i].off[TEMP_SENSOR_3_CHARGER]) {
				current_level = i - 1;
			} else
				break;
		}
	} else if (temp[TEMP_SENSOR_1_CPU] > prev_tmp[TEMP_SENSOR_1_CPU] ||
		   temp[TEMP_SENSOR_2_5V] > prev_tmp[TEMP_SENSOR_2_5V] ||
		   temp[TEMP_SENSOR_3_CHARGER] > prev_tmp[TEMP_SENSOR_3_CHARGER]) {
		for (i = current_level; i < num_fan_levels; i++) {
			if (temp[TEMP_SENSOR_1_CPU] >
				     fan_step_table[i].on[TEMP_SENSOR_1_CPU] ||
			     (temp[TEMP_SENSOR_2_5V] >
				     fan_step_table[i].on[TEMP_SENSOR_2_5V] &&
			    temp[TEMP_SENSOR_3_CHARGER] >
				    fan_step_table[i].on[TEMP_SENSOR_3_CHARGER])) {
				current_level = i + 1;
			} else
				break;
		}
	}

	if (current_level < 0)
		current_level = 0;

	if (current_level >= num_fan_levels)
		current_level = num_fan_levels - 1;

	for (i = 0; i < TEMP_SENSOR_COUNT; ++i)
		prev_tmp[i] = temp[i];

	new_rpm = fan_step_table[current_level].rpm[0];

	ccprintf("current_level:%d, new_rpm:%d\n", current_level, new_rpm);
	ccprintf("temp[TEMP_SENSOR_1_CPU]:%d\n",temp[TEMP_SENSOR_1_CPU]);
	ccprintf("temp[TEMP_SENSOR_2_5V]:%d\n",temp[TEMP_SENSOR_2_5V]);
	ccprintf("temp[TEMP_SENSOR_3_CHARGER]:%d\n",temp[TEMP_SENSOR_3_CHARGER]);
	ccprintf("------------------------------\n");

	return new_rpm;

}
void board_override_fan_control(int fan, int *temp)
{
	if (chipset_in_state(CHIPSET_STATE_ON | CHIPSET_STATE_ANY_SUSPEND)) {
		fan_set_rpm_mode(0, 1);
		fan_set_rpm_target(0, fan_table_to_rpm(0, temp));
	}
}
