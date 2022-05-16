/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "common.h"
#include "fan_chip.h"
#include "fan.h"
#include "host_command.h"
#include "temp_sensor.h"
#include "thermal.h"
#include "util.h"

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
	.ch = MFT_CH_0,	/* Use MFT id to control fan */
	.pgood_gpio = -1,
	.enable_gpio = GPIO_EN_PP5000_FAN,
};
static const struct fan_rpm fan_rpm_0 = {
	.rpm_min = 3500,
	.rpm_start = 3500,
	.rpm_max = 4300,
};

static const struct fan_rpm fan_rpm_1 = {
	.rpm_min = 4300,
	.rpm_start = 4300,
	.rpm_max = 4700,
};

struct fan_t fans[FAN_CH_COUNT] = {
	[FAN_CH_0] = {
		.conf = &fan_conf_0,
		.rpm = &fan_rpm_0,
	},
};

void fan_set_percent(int fan, int pct, int fan_rpm)
{
	int actual_rpm;
	int new_rpm;
	int min_rpm;
	if (fan_rpm == 1){
		fans[fan].rpm = &fan_rpm_1;
		min_rpm = fans[fan].rpm->rpm_min;
	}
	else{
		fans[fan].rpm = &fan_rpm_0;
		min_rpm = fans[fan].rpm->rpm_min;
	}
	new_rpm = fan_percent_to_rpm(fan, pct);
	actual_rpm = fan_get_rpm_actual(FAN_CH(fan));
	if (new_rpm &&
	    actual_rpm < min_rpm &&
	    new_rpm < fans[fan].rpm->rpm_start)
		new_rpm = fans[fan].rpm->rpm_start;

	fan_set_rpm_target(FAN_CH(fan), new_rpm);
}

void board_override_fan_control(int fan, int *tmp)
{
	int f;
	int sensor_1;
	int sensor_4;
	int sensor_3;
	int fan_rpm = 0;
	/* figure out the max fan needed */

	sensor_1 = thermal_fan_percent(thermal_params[0].temp_fan_off,
				thermal_params[0].temp_fan_max,
				C_TO_K(tmp[0]));
	sensor_4 = thermal_fan_percent(thermal_params[2].temp_fan_off,
				thermal_params[2].temp_fan_max,
				C_TO_K(tmp[2]));
	sensor_3 = thermal_fan_percent(thermal_params[1].temp_fan_off,
				thermal_params[1].temp_fan_max,
				C_TO_K(tmp[1]));
	if (sensor_3 > 0){
		f = sensor_3;
		fan_rpm = 1;
	}
	else{
		if (sensor_1 > sensor_4){
			f = sensor_4;
		}
		else{
			f = sensor_1;
		}
	}

	/* transfer percent to rpm */
	fan_set_percent(fan, f, fan_rpm);
}