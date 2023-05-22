/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "fan.h"
#include "math_util.h"
#include "tablet_mode.h"
#include "temp_sensor/temp_sensor.h"
#include "timer.h"
#include "thermal.h"

#include <zephyr/kernel.h>

#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_THERMAL, format, ##args)

K_TIMER_DEFINE(grace_period_timer, NULL, NULL);

#define TEMP_1_SOC TEMP_SENSOR_ID(DT_NODELABEL(soc_temp))
#define TEMP_2_DDR TEMP_SENSOR_ID(DT_NODELABEL(ddr_temp))
#define TEMP_3_CHARGER TEMP_SENSOR_ID(DT_NODELABEL(charger_temp))
#define TEMP_4_AMBIENT TEMP_SENSOR_ID(DT_NODELABEL(ambient_temp))
#define RECORD_TIME (2 * MINUTE)

enum thermal_cfg_table { LAPTOP_MODE, TABLET_MODE, THERMAL_CFG_TABLE_COUNT };

enum fan_rpm_table {
	RPM_TABLE_CPU,
	RPM_TABLE_CPU_TABLET,
	RPM_TABLE_DDR,
	RPM_TABLE_CHARGER,
	RPM_TABLE_AMBIENT,
	FAN_RPM_TABLE_COUNT
};

struct thermal_policy_config {
	uint8_t fan_off_slop1;
	uint8_t fan_max_slop1;
	uint8_t fan_off_slop2;
	uint8_t fan_max_slop2;
	uint8_t fan_slop_threshold;
	uint8_t ddr_fan_turn_on;
	uint8_t ddr_fan_turn_off;
	uint8_t rpm_table_cpu;
};

static const struct fan_rpm rpm_table[5] = {
	[RPM_TABLE_CPU] = {
		.rpm_min = 0,
		.rpm_start = 0,
		.rpm_max = 4000,
	},

	[RPM_TABLE_CPU_TABLET] = {
		.rpm_min = 0,
		.rpm_start = 0,
		.rpm_max = 4000,
	},

	[RPM_TABLE_DDR] = {
		.rpm_min = 4000,
		.rpm_start = 4000,
		.rpm_max = 4200,
	},

	[RPM_TABLE_CHARGER] = {
		.rpm_min = 4000,
		.rpm_start = 4000,
		.rpm_max = 4200,
	},

	[RPM_TABLE_AMBIENT] = {
		.rpm_min = 4000,
		.rpm_start = 4000,
		.rpm_max = 4200,
	},
};

static const struct thermal_policy_config
	thermal_cfg[THERMAL_CFG_TABLE_COUNT] = {
	[LAPTOP_MODE] = {
		.fan_off_slop1 = 24,
		.fan_max_slop1 = 51,
		.fan_off_slop2 = 29,
		.fan_max_slop2 = 48,
		.fan_slop_threshold = 45,
		.ddr_fan_turn_off = 38,
		.ddr_fan_turn_on = 44,
		.rpm_table_cpu = RPM_TABLE_CPU,
	},

	[TABLET_MODE] = {
		.fan_off_slop1 = 25,
		.fan_max_slop1 = 52,
		.fan_off_slop2 = 30,
		.fan_max_slop2 = 49,
		.fan_slop_threshold = 45,
		.ddr_fan_turn_off = 38,
		.ddr_fan_turn_on = 44,
		.rpm_table_cpu = RPM_TABLE_CPU_TABLET,
	},
};

struct temp_sensor {
	/*
	 * Sensor 1~3 trigger point, set -1 if we're not using this
	 * sensor to determine fan speed.
	 */
	int8_t off;
	/*
	 * Sensor 1~3 trigger point, set -1 if we're not using this
	 * sensor to determine fan speed.
	 */
	int8_t max;
};

#define TEMP_SENSOR_ENTRY(nd)                     \
	{                                       \
		.off = DT_PROP(nd, temp_fan_off),     \
		.max = DT_PROP(nd, temp_fan_max),   \
	},

static struct temp_sensor temp_sensor_table[] = { DT_FOREACH_CHILD(
	DT_INST(0, cros_ec_temp_sensors), TEMP_SENSOR_ENTRY) };

static void fan_get_rpm(int fan)
{
	static timestamp_t deadline;

	/* Record actual RPM every 2 minutes. */
	if (timestamp_expired(deadline, NULL)) {
		ccprints("fan actual rpm: %d", fan_get_rpm_actual(FAN_CH(fan)));
		deadline.val = get_time().val + RECORD_TIME;
	}
}

static void fan_set_percent(int fan, int pct)
{
	int new_rpm;

	new_rpm = fan_percent_to_rpm(fan, pct);
	fan_set_rpm_target(FAN_CH(fan), new_rpm);
	fan_get_rpm(fan);
}

void board_override_fan_control(int fan, int *tmp)
{
	/*
	 * Crota's fan speed is control by four sensors.
	 *
	 * Sensor charger control the speed when system's temperature
	 * is too high.
	 * Other sensors control normal loading's speed.
	 *
	 * When sensor charger is triggered, the fan speed is only
	 * control by sensor charger, avoid heat damage to system.
	 * When other sensors is triggered, the fan is control
	 * by other sensors.
	 *
	 * Sensor SOC has two slopes for fan speed.
	 * Sensor DDR also become a fan on/off switch.
	 */
	const struct thermal_policy_config *t;
	static int pct;
	int i;
	int fan_pct[TEMP_SENSOR_COUNT];
	int fan_off;
	int fan_max;

    /* Decide is tablet mode or laptop mode. */
	if (tablet_get_mode())
		t = &thermal_cfg[TABLET_MODE];
	else
		t = &thermal_cfg[LAPTOP_MODE];
    
    /* Decide sensor SOC temperature using which slope. */
	if (tmp[TEMP_1_SOC] <= t->fan_slop_threshold) {
		fan_off = t->fan_off_slop1;
		fan_max = t->fan_max_slop1;
	} else {
		fan_off = t->fan_off_slop2;
		fan_max = t->fan_max_slop2;
	}
    temp_sensor_table[TEMP_1_SOC].off = C_TO_K(fan_off);
	temp_sensor_table[TEMP_1_SOC].max = C_TO_K(fan_max);

    for (i = 0; i < TEMP_SENSOR_COUNT; i++) {
		fan_pct[i] = thermal_fan_percent(temp_sensor_table[i].off,
						 temp_sensor_table[i].max,
						 C_TO_K(tmp[i]));
	}

    if (((tmp[TEMP_2_DDR]) <= t->ddr_fan_turn_on && pct == 0) ||
	    ((tmp[TEMP_2_DDR]) < t->ddr_fan_turn_off))
		pct = 0;
	else {
		/*
		 * Decide which sensor was triggered and choose table.
		 * Priority: charger > soc > ddr > ambient
		 */
		if (fan_pct[TEMP_3_CHARGER]) {
			fans[fan].rpm = &rpm_table[RPM_TABLE_CHARGER];
			pct = fan_pct[TEMP_3_CHARGER];
		} else if (fan_pct[TEMP_1_SOC]) {
			fans[fan].rpm = &rpm_table[t->rpm_table_cpu];
			pct = fan_pct[TEMP_1_SOC];
		} else if (fan_pct[TEMP_2_DDR]) {
			fans[fan].rpm = &rpm_table[RPM_TABLE_DDR];
			pct = fan_pct[TEMP_2_DDR];
		} else {
			fans[fan].rpm = &rpm_table[RPM_TABLE_AMBIENT];
			pct = fan_pct[TEMP_4_AMBIENT];
		}
	}
    fan_set_percent(fan, pct);
}
