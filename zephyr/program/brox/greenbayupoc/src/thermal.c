/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "common.h"
#include "fan.h"
#include "temp_sensor/temp_sensor.h"
#include "thermal.h"
#include "util.h"
#include "console.h"

#include <ap_power/ap_power_interface.h>

#define TEMP_CPU TEMP_SENSOR_ID(DT_NODELABEL(temp_ddr_soc))
#define TEMP_5V TEMP_SENSOR_ID(DT_NODELABEL(temp_fan_inlet))

static int g_fake_temp[TEMP_SENSOR_COUNT];
static int g_temp_override_enabled;

struct fan_step {
	/*
	 * Sensor 1~3 trigger point, set -1 if we're not using this
	 * sensor to determine fan speed.
	 */
	int8_t on[TEMP_SENSOR_COUNT];
	/*
	 * Sensor 1~3 trigger point, set -1 if we're not using this
	 * sensor to determine fan speed.
	 */
	int8_t off[TEMP_SENSOR_COUNT];
	/* Fan rpm */
	uint16_t rpm[FAN_CH_COUNT];
};

#define FAN_TABLE_ENTRY(nd)                     \
	{                                       \
		.on = DT_PROP(nd, temp_on),     \
		.off = DT_PROP(nd, temp_off),   \
		.rpm = DT_PROP(nd, rpm_target), \
	},

static const struct fan_step fan_step_table[] = { DT_FOREACH_CHILD(
	DT_INST(0, cros_ec_fan_steps), FAN_TABLE_ENTRY) };

/* current fan level */
static int current_level; // Move current_level to global scope (or file scope)
/* previous sensor temperature */
static int prev_tmp[TEMP_SENSOR_COUNT];

int fan_table_to_rpm(int fan, int *temp)
{
        int i;
        /*
         * Compare the current and previous temperature, we have
         * the three paths :
         * 1. decreasing path. (check the release point)
         * 2. increasing path. (check the trigger point)
         * 3. invariant path. (return the current RPM)
         *
         * Domika thermal table V1-1
         * Increase path judgment: CPU || (5V && Charger)
         * Decrease path judgment: CPU && 5V && Charger
         */
        if (temp[TEMP_CPU] < prev_tmp[TEMP_CPU] ||
            temp[TEMP_5V] < prev_tmp[TEMP_5V]) {
                for (i = current_level; i > 0; i--) {
                        if (temp[TEMP_CPU] < fan_step_table[i].off[TEMP_CPU] &&
                            temp[TEMP_5V] < fan_step_table[i].off[TEMP_5V]) {
                                current_level = i - 1;
                        } else
                                break;
                }
        } else if (temp[TEMP_CPU] > prev_tmp[TEMP_CPU] ||
                   temp[TEMP_5V] > prev_tmp[TEMP_5V]) {
                for (i = current_level; i < ARRAY_SIZE(fan_step_table); i++) {
                        if (temp[TEMP_CPU] > fan_step_table[i].on[TEMP_CPU] ||
                            (temp[TEMP_5V] > fan_step_table[i].on[TEMP_5V])) {
                                current_level = i + 1;
                        } else
                                break;
                }
        }
        if (current_level < 0)
                current_level = 0;

        if (current_level >= ARRAY_SIZE(fan_step_table))
                current_level = ARRAY_SIZE(fan_step_table) - 1;

        for (i = 0; i < TEMP_SENSOR_COUNT; ++i)
                prev_tmp[i] = temp[i];

        return fan_step_table[current_level].rpm[fan];
}

// New function to get the current fan level
int get_fan_current_level(void) {
    return current_level;
}


__override int board_override_fan_control(int fan, const int *temps)
{
        if (!g_temp_override_enabled)
                return 0;

        fan_set_rpm_mode(fan, 1);
        int rpm = fan_table_to_rpm(fan, g_fake_temp);
        fan_set_rpm_target(fan, rpm);

        return 1;
}


static int cmd_set_temp(int argc, const char **argv)
{
        if (argc != 2) {
                ccprintf("Usage: tempset <temp|auto>\n");
                return EC_ERROR_PARAM_COUNT;
        }

        if (!strcasecmp(argv[1], "auto")) {
                g_temp_override_enabled = 0;
                ccprintf("Fan control set to AUTO (EC default)\n");
                return EC_SUCCESS;
        }

        char *e;
        int temp = strtoi(argv[1], &e, 0);
        if (*e) {
                ccprintf("Invalid number: %s\n", argv[1]);
                return EC_ERROR_PARAM1;
        }

        g_temp_override_enabled = 1;
        g_fake_temp[TEMP_CPU] = temp;
        g_fake_temp[TEMP_5V] = temp;

        fan_set_rpm_mode(0, 1);
        int rpm = fan_table_to_rpm(0, g_fake_temp);
        int level = get_fan_current_level(); // Get current level
        fan_set_rpm_target(0, rpm);

        ccprintf("Set fake temp = %d -> target RPM = %d (level = %d)\n", temp, rpm, level); // Print level
        return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(tempset, cmd_set_temp,
        "<temp>",
        "Set fake temp and override fan control");