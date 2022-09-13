/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Physical fans. These are logically separate from pwm_channels. */

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "fan_chip.h"
#include "fan.h"
#include "hooks.h"
#include "pwm.h"
#include "tablet_mode.h"
#include "timer.h"
#include "thermal.h"
#include "util.h"

#define SENSOR_SOC_FAN_OFF_SLOP1 30
#define SENSOR_SOC_FAN_MAX_SLOP1 52
#define SENSOR_SOC_FAN_OFF_SLOP2 16
#define SENSOR_SOC_FAN_MAX_SLOP2 57
#define SENSOR_SOC_FAN_OFF_SLOP1_TABLET 31
#define SENSOR_SOC_FAN_MAX_SLOP1_TABLET 53
#define SENSOR_SOC_FAN_OFF_SLOP2_TABLET 17
#define SENSOR_SOC_FAN_MAX_SLOP2_TABLET 58
#define SENSOR_SOC_FAN_SLOP_THRESHOLD 47
#define SENSOR_DDR_FAN_TURN_OFF 39
#define SENSOR_DDR_FAN_TURN_ON 41
#define SENSOR_DDR_FAN_TURN_OFF_TABLET 40
#define SENSOR_DDR_FAN_TURN_ON_TABLET 42
#define RECORD_TIME (2 * MINUTE)

/* MFT channels. These are logically separate from pwm_channels. */
const struct mft_t mft_channels[] = {
	[MFT_CH_0] = {
		.module = NPCX_MFT_MODULE_1,
		.clk_src = TCKC_LFCLK,
		.pwm_id = PWM_CH_FAN,
	},
};
BUILD_ASSERT(ARRAY_SIZE(mft_channels) == MFT_CH_COUNT);

static const struct fan_conf fan_conf_0 = {
	.flags = FAN_USE_RPM_MODE,
	.ch = MFT_CH_0, /* Use MFT id to control fan */
	.pgood_gpio = -1,
	.enable_gpio = GPIO_EN_PP5000_FAN,
};

static const struct fan_rpm rpm_table[FAN_RPM_TABLE_COUNT] = {
	[RPM_TABLE_CPU] = {
		.rpm_min = 2200,
		.rpm_start = 2200,
		.rpm_max = 4200,
	},

	[RPM_TABLE_CPU_TABLET] = {
		.rpm_min = 2200,
		.rpm_start = 2200,
		.rpm_max = 4200,
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

struct fan_t fans[FAN_CH_COUNT] = {
	[FAN_CH_0] = {
		.conf = &fan_conf_0,
		.rpm = &rpm_table[RPM_TABLE_CPU],
	},
};

static void fan_get_rpm(int fan)
{
	static timestamp_t deadline;

	/* Record actual RPM every 2 minutes. */
	if (timestamp_expired(deadline, NULL)) {
		ccprints("fan actual rpm: %d", fan_get_rpm_actual(FAN_CH(fan)));
		deadline.val += RECORD_TIME;
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
	static int pct;
	int sensor_soc;
	int sensor_ddr;
	int sensor_charger;
	int sensor_ambient;
	int fan_off_slop1;
	int fan_max_slop1;
	int fan_off_slop2;
	int fan_max_slop2;
	int ddr_fan_turn_on;
	int ddr_fan_turn_off;
	int rpm_table_cpu;

	/* Decide is tablet mode or not. */
	if (tablet_get_mode()) {
		fan_off_slop1 = SENSOR_SOC_FAN_OFF_SLOP1_TABLET;
		fan_max_slop1 = SENSOR_SOC_FAN_MAX_SLOP1_TABLET;
		fan_off_slop2 = SENSOR_SOC_FAN_OFF_SLOP2_TABLET;
		fan_max_slop2 = SENSOR_SOC_FAN_MAX_SLOP2_TABLET;
		ddr_fan_turn_off = SENSOR_DDR_FAN_TURN_OFF_TABLET;
		ddr_fan_turn_on = SENSOR_DDR_FAN_TURN_ON_TABLET;
		rpm_table_cpu = RPM_TABLE_CPU_TABLET;
	} else {
		fan_off_slop1 = SENSOR_SOC_FAN_OFF_SLOP1;
		fan_max_slop1 = SENSOR_SOC_FAN_MAX_SLOP1;
		fan_off_slop2 = SENSOR_SOC_FAN_OFF_SLOP2;
		fan_max_slop2 = SENSOR_SOC_FAN_MAX_SLOP2;
		ddr_fan_turn_off = SENSOR_DDR_FAN_TURN_OFF;
		ddr_fan_turn_on = SENSOR_DDR_FAN_TURN_ON;
		rpm_table_cpu = RPM_TABLE_CPU;
	}

	/* Decide sensor SOC temperature using which slope. */
	if (tmp[TEMP_SENSOR_1_SOC] <= SENSOR_SOC_FAN_SLOP_THRESHOLD) {
		thermal_params[TEMP_SENSOR_1_SOC].temp_fan_off =
			C_TO_K(fan_off_slop1);
		thermal_params[TEMP_SENSOR_1_SOC].temp_fan_max =
			C_TO_K(fan_max_slop1);
	} else {
		thermal_params[TEMP_SENSOR_1_SOC].temp_fan_off =
			C_TO_K(fan_off_slop2);
		thermal_params[TEMP_SENSOR_1_SOC].temp_fan_max =
			C_TO_K(fan_max_slop2);
	}

	sensor_soc = thermal_fan_percent(
		thermal_params[TEMP_SENSOR_1_SOC].temp_fan_off,
		thermal_params[TEMP_SENSOR_1_SOC].temp_fan_max,
		C_TO_K(tmp[TEMP_SENSOR_1_SOC]));
	sensor_ddr = thermal_fan_percent(
		thermal_params[TEMP_SENSOR_2_DDR].temp_fan_off,
		thermal_params[TEMP_SENSOR_2_DDR].temp_fan_max,
		C_TO_K(tmp[TEMP_SENSOR_2_DDR]));
	sensor_charger = thermal_fan_percent(
		thermal_params[TEMP_SENSOR_3_CHARGER].temp_fan_off,
		thermal_params[TEMP_SENSOR_3_CHARGER].temp_fan_max,
		C_TO_K(tmp[TEMP_SENSOR_3_CHARGER]));
	sensor_ambient = thermal_fan_percent(
		thermal_params[TEMP_SENSOR_4_AMBIENT].temp_fan_off,
		thermal_params[TEMP_SENSOR_4_AMBIENT].temp_fan_max,
		C_TO_K(tmp[TEMP_SENSOR_4_AMBIENT]));

	/*
	 * Sensor DDR turn on when temperature > 38,
	 * turn off when temperature < 37
	 */
	if ((tmp[TEMP_SENSOR_2_DDR]) < ddr_fan_turn_off) {
		pct = 0;
	} else if ((tmp[TEMP_SENSOR_2_DDR]) > ddr_fan_turn_on) {
		/*
		 * Decide which sensor was triggered and choose table.
		 * Priority: charger > soc > ddr > ambient
		 */
		if (sensor_charger) {
			fans[fan].rpm = &rpm_table[RPM_TABLE_CHARGER];
			pct = sensor_charger;
		} else if (sensor_soc) {
			fans[fan].rpm = &rpm_table[rpm_table_cpu];
			pct = sensor_soc;
		} else if (sensor_ddr) {
			fans[fan].rpm = &rpm_table[RPM_TABLE_DDR];
			pct = sensor_ddr;
		} else {
			fans[fan].rpm = &rpm_table[RPM_TABLE_AMBIENT];
			pct = sensor_ambient;
		}
	}

	/* Transfer percent to rpm. */
	fan_set_percent(fan, pct);
}
