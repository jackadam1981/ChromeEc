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

struct fan_step fan_table_clamshell[] = {
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

struct fan_step fan_table_tablet[] = {
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

/*****************************************************************************/
/* EC-specific thermal controls */

test_mockable_static void smi_sensor_failure_warning(void)
{
	CPRINTS("can't read any temp sensors!");
	host_set_single_event(EC_HOST_EVENT_THERMAL);
}

int fan_table_to_rpm(int fan, int *temp)
{
	static int current_level;
	static int prev_temp[TEMP_SENSOR_COUNT];
	static int new_rpm;
	struct fan_step *ptr;
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

	if (temp[TEMP_SENSOR_1] < prev_temp[TEMP_SENSOR_1] ||
		temp[TEMP_SENSOR_3] < prev_temp[TEMP_SENSOR_3] ||
		temp[TEMP_SENSOR_4] < prev_temp[TEMP_SENSOR_4]) {
		for (i = current_level; i > 0; i--) {
			if ((temp[TEMP_SENSOR_1] < ptr[i].off[TEMP_SENSOR_1] &&
				 temp[TEMP_SENSOR_4] < ptr[i].off[TEMP_SENSOR_4]) ||
				 temp[TEMP_SENSOR_3] < ptr[i].off[TEMP_SENSOR_3])
				current_level = i - 1;
			else
				break;
		}
	} else if (temp[TEMP_SENSOR_1] > prev_temp[TEMP_SENSOR_1] ||
		   temp[TEMP_SENSOR_3] > prev_temp[TEMP_SENSOR_3] ||
		   temp[TEMP_SENSOR_4] > prev_temp[TEMP_SENSOR_4]) {
		for (i = current_level; i < NUM_FAN_LEVELS; i++) {
			if ((temp[TEMP_SENSOR_1] > ptr[i].on[TEMP_SENSOR_1] &&
				 temp[TEMP_SENSOR_4] > ptr[i].on[TEMP_SENSOR_4]) &&
				 temp[TEMP_SENSOR_3] > ptr[i].on[TEMP_SENSOR_3])
				current_level = i + 1;
			else
				break;
		}
	}

	if (current_level < 0)
		current_level = 0;

	for (i = 0; i < TEMP_SENSOR_COUNT; ++i)
		prev_temp[i] = temp[i];

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

static void thermal_control(void)
{
	int i, t, rv;
	int num_sensors_read;
	int temp_fan_configured;
	int temp_all[TEMP_SENSOR_COUNT];
	int new_rpm;
	int fan;

	/* Get ready to count things */
	num_sensors_read = 0;
	temp_fan_configured = 0;
	new_rpm = 0;

	/* go through all the sensors */
	for (i = 0; i < TEMP_SENSOR_COUNT; ++i) {

		/* read one */
		rv = temp_sensor_read(i, &t);
		temp_all[i] = K_TO_C(t);

		if (rv != EC_SUCCESS)
			continue;
		else
			num_sensors_read++;

		temp_fan_configured = 1;
	}

	if (!num_sensors_read) {
		/*
		 * Trigger a SMI event if we can't read any sensors.
		 *
		 * In theory we could do something more elaborate like forcing
		 * the system to shut down if no sensors are available after
		 * several retries.  This is a very unlikely scenario -
		 * particularly on LM4-based boards, since the LM4 has its own
		 * internal temp sensor.  It's most likely to occur during
		 * bringup of a new board, where we haven't debugged the I2C
		 * bus to the sensors; forcing a shutdown in that case would
		 * merely hamper board bringup.
		 *
		 * If in G3, then there is no need trigger an SMI event since
		 * the AP is off and this can be an expected state if
		 * temperature sensors are powered by a power rail that's only
		 * on if the AP is out of G3. Note this could be 'ANY_OFF' as
		 * well, but that causes the thermal unit test to fail.
		 */
		if (!chipset_in_state(CHIPSET_STATE_HARD_OFF))
			smi_sensor_failure_warning();
		return;
	}

	if (chipset_in_state(CHIPSET_STATE_ON)) {
		if (temp_fan_configured) {
			for (fan = 0; fan < fan_get_count(); fan++) {
				if (!thermal_control_enabled[fan])
					return;

				fan_set_rpm_mode(FAN_CH(fan), 1);
				new_rpm = fan_table_to_rpm(fan, (int *)temp_all);
				fan_set_rpm_target(FAN_CH(fan), new_rpm);
			}
		}
	}
}

/* Wait until after the sensors have been read */
DECLARE_HOOK(HOOK_SECOND, thermal_control, HOOK_PRIO_TEMP_SENSOR_DONE);
