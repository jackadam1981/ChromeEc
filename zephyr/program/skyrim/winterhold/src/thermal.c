/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "fan.h"
#include "hooks.h"
#include "host_command.h"
#include "temp_sensor/temp_sensor.h"
#include "thermal.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_THERMAL, format, ##args)

#define FAN_TABLE_ENTRY(nd)                     \
	{                                       \
		.on = DT_PROP(nd, temp_on),     \
		.off = DT_PROP(nd, temp_off),   \
		.rpm = DT_PROP(nd, rpm_target), \
	},

/*AMB sensor for thermal table control*/
#define TEMP_AMB TEMP_SENSOR_ID(DT_NODELABEL(temp_sensor_amb))

/*SOC and CPU sensor for fan table control*/
#define TEMP_SOC TEMP_SENSOR_ID(DT_NODELABEL(temp_sensor_soc))
#define TEMP_CPU TEMP_SENSOR_ID(DT_NODELABEL(temp_sensor_cpu))

struct fan_step {
	/*
	 * Sensor 0~4 trigger point, set -1 if we're not using this
	 * sensor to determine fan speed.
	 */
	int on[TEMP_SENSOR_COUNT];
	/*
	 * Sensor 0~4 release point, set -1 if we're not using this
	 * sensor to determine fan speed.
	 */
	int off[TEMP_SENSOR_COUNT];
	/* Fan rpm */
	uint16_t rpm[FAN_CH_COUNT];
};

static const struct fan_step fan_table[] = { DT_FOREACH_CHILD(
	DT_NODELABEL(fan_step_table), FAN_TABLE_ENTRY) };
#define NUM_FAN_LEVELS ARRAY_SIZE(fan_table)

static int last_amb_temp = -1;

/* Set SCI event to host for temperature change */
static void detect_temp_change(void)
{
	int t, rv;

	rv = temp_sensor_read(TEMP_AMB, &t);
	if (rv == EC_SUCCESS) {
		if (last_amb_temp != t) {
			last_amb_temp = t;
			host_set_single_event(EC_HOST_EVENT_THERMAL_THRESHOLD);
		}
	} else if (rv == EC_ERROR_INVAL) {
		CPRINTS("Temp sensor: Invalid id");
	}
}
DECLARE_HOOK(HOOK_SECOND, detect_temp_change, HOOK_PRIO_TEMP_SENSOR_DONE);

static int fan_table_to_rpm(int fan, int *temp)
{
	static int prev_temp[TEMP_SENSOR_COUNT];
	bool decreasing, increasing;
	static int current_level;
	int i;

	/*
	 * Compare the current and previous temperature, we have
	 * the three paths :
	 *  1. decreasing path. (check the release point - AND over sensors)
	 *  2. increasing path. (check the trigger point - OR over sensors)
	 *  3. invariant path. (return the current RPM)
	 */

	if (temp[TEMP_SOC] < prev_temp[TEMP_SOC] ||
	    temp[TEMP_CPU] < prev_temp[TEMP_CPU])
		decreasing = true;

	if (temp[TEMP_SOC] > prev_temp[TEMP_SOC] ||
	    temp[TEMP_CPU] > prev_temp[TEMP_CPU])
		increasing = true;

	if (!increasing && !decreasing)
		return fan_table[current_level].rpm[fan];

	if (decreasing) {
		for (i = current_level; i > 0; i--) {
			if (temp[TEMP_SOC] <= fan_table[i].off[TEMP_SOC] &&
			    temp[TEMP_CPU] <= fan_table[i].off[TEMP_CPU]) {
				current_level = i - 1;
				CPRINTS("switching level to %d, rpm %d",
					current_level,
					fan_table[current_level].rpm[fan]);
			}
		}
	}
	if (increasing) {
		for (i = current_level; i < NUM_FAN_LEVELS; i++) {
			if (temp[TEMP_SOC] >= fan_table[i].on[TEMP_SOC] ||
			    temp[TEMP_CPU] >= fan_table[i].on[TEMP_CPU]) {
				current_level = i;
				CPRINTS("switching level to %d, rpm %d",
					current_level,
					fan_table[current_level].rpm[fan]);
			}
		}
	}

	if (current_level < 0)
		current_level = 0;

	if (current_level >= NUM_FAN_LEVELS)
		current_level = NUM_FAN_LEVELS - 1;

	prev_temp[TEMP_SOC] = temp[TEMP_SOC];
	prev_temp[TEMP_CPU] = temp[TEMP_CPU];

	return fan_table[current_level].rpm[fan];
}

void board_override_fan_control(int fan, int *temp)
{
	/*
	 * In common/fan.c pwm_fan_stop() will turn off fan
	 * when chipset suspend or shutdown.
	 */
	if (chipset_in_state(CHIPSET_STATE_ON)) {
		fan_set_rpm_mode(fan, 1);
		fan_set_rpm_target(fan, fan_table_to_rpm(fan, temp));
	}
}
