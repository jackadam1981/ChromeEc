/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "body_detection.h"
#include "fan.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_switch.h"
#include "math_util.h"
#include "temp_sensor/temp_sensor.h"
#include "thermal.h"

#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_THERMAL, format, ##args)

/*AMB sensor for thermal tabel control*/
#define TEMP_AMB TEMP_SENSOR_ID(DT_NODELABEL(temp_sensor_amb))
/*SOC and CPU sensor for fan tabel control*/
#define TEMP_SOC TEMP_SENSOR_ID(DT_NODELABEL(temp_sensor_soc))
#define TEMP_CPU TEMP_SENSOR_ID(DT_NODELABEL(temp_sensor_cpu))

/*
 * TODO(b/202062363): Remove when clang is fixed.
 */
#define THERMAL_DESKTOP_LID_OPEN \
	{                        \
		.temp_host = { \
			[EC_TEMP_THRESH_WARN] = C_TO_K(43), \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(97), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(98), \
		}, \
		.temp_host_release = { \
			[EC_TEMP_THRESH_WARN] = C_TO_K(39), \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(87), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(88), \
		}, \
	}
__maybe_unused static const struct ec_thermal_config thermal_desktop_lid_open =
	THERMAL_DESKTOP_LID_OPEN;

/*
 * TODO(b/202062363): Remove when clang is fixed.
 */
#define THERMAL_DESKTOP_LID_CLOSE \
	{                         \
		.temp_host = { \
			[EC_TEMP_THRESH_WARN] = C_TO_K(43), \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(97), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(98), \
		}, \
		.temp_host_release = { \
			[EC_TEMP_THRESH_WARN] = C_TO_K(39), \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(87), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(88), \
		},  \
	}
__maybe_unused static const struct ec_thermal_config thermal_desktop_lid_close =
	THERMAL_DESKTOP_LID_CLOSE;

/*
 * TODO(b/202062363): Remove when clang is fixed.
 */
#define THERMAL_LAPTOP           \
	{                        \
		.temp_host = { \
			[EC_TEMP_THRESH_WARN] = C_TO_K(42), \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(97), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(98), \
		}, \
		.temp_host_release = { \
			[EC_TEMP_THRESH_WARN] = C_TO_K(38), \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(87), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(88), \
		}, \
	}
__maybe_unused static const struct ec_thermal_config thermal_laptop =
	THERMAL_LAPTOP;

/*
 * TODO(b/202062363): Remove when clang is fixed.
 */
#define FAN_SOC_DESKTOP_LID_OPEN \
	{                        \
		.temp_host = { \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(97), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(98), \
		}, \
		.temp_host_release = { \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(87), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(88), \
		}, \
		.temp_fan_off = C_TO_K(55), \
		.temp_fan_max = C_TO_K(72), \
	}
__maybe_unused static const struct ec_thermal_config fan_soc_desktop_lid_open =
	FAN_SOC_DESKTOP_LID_OPEN;

/*
 * TODO(b/202062363): Remove when clang is fixed.
 */
#define FAN_SOC_DESKTOP_LID_CLOSE \
	{                         \
		.temp_host = { \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(97), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(98), \
		}, \
		.temp_host_release = { \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(87), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(88), \
		}, \
		.temp_fan_off = C_TO_K(55), \
		.temp_fan_max = C_TO_K(72),  \
	}
__maybe_unused static const struct ec_thermal_config fan_soc_desktop_lid_close =
	FAN_SOC_DESKTOP_LID_CLOSE;

/*
 * TODO(b/202062363): Remove when clang is fixed.
 */
#define FAN_SOC_LAPTOP           \
	{                        \
		.temp_host = { \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(97), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(98), \
		}, \
		.temp_host_release = { \
			[EC_TEMP_THRESH_HIGH] = C_TO_K(87), \
			[EC_TEMP_THRESH_HALT] = C_TO_K(88), \
		}, \
		.temp_fan_off = C_TO_K(51), \
		.temp_fan_max = C_TO_K(68), \
	}
__maybe_unused static const struct ec_thermal_config fan_soc_laptop =
	FAN_SOC_LAPTOP;

/*
 * TODO(b/202062363): Remove when clang is fixed.
 */
#define FAN_CPU_DESKTOP_LID_OPEN                                        \
	{                                                               \
		.temp_fan_off = C_TO_K(76), .temp_fan_max = C_TO_K(82), \
	}
__maybe_unused static const struct ec_thermal_config fan_cpu_desktop_lid_open =
	FAN_CPU_DESKTOP_LID_OPEN;

/*
 * TODO(b/202062363): Remove when clang is fixed.
 */
#define FAN_CPU_DESKTOP_LID_CLOSE                                       \
	{                                                               \
		.temp_fan_off = C_TO_K(76), .temp_fan_max = C_TO_K(82), \
	}
__maybe_unused static const struct ec_thermal_config fan_cpu_desktop_lid_close =
	FAN_CPU_DESKTOP_LID_CLOSE;

/*
 * TODO(b/202062363): Remove when clang is fixed.
 */
#define FAN_CPU_LAPTOP                                                  \
	{                                                               \
		.temp_fan_off = C_TO_K(72), .temp_fan_max = C_TO_K(78), \
	}
__maybe_unused static const struct ec_thermal_config fan_cpu_laptop =
	FAN_CPU_LAPTOP;

static int last_amb_temp = -1;

/* Switch thermal table when mode change */
static void thermal_table_switch(void)
{
	enum body_detect_states body_state = body_detect_get_state();

	if (body_state == BODY_DETECTION_OFF_BODY) {
		if (lid_is_open()) {
			thermal_params[TEMP_AMB] = thermal_desktop_lid_open;
			thermal_params[TEMP_SOC] = fan_soc_desktop_lid_open;
			thermal_params[TEMP_CPU] = fan_cpu_desktop_lid_open;
			CPRINTS("Thermal: Desktop lid open mode");
		} else {
			thermal_params[TEMP_AMB] = thermal_desktop_lid_close;
			thermal_params[TEMP_SOC] = fan_soc_desktop_lid_close;
			thermal_params[TEMP_CPU] = fan_cpu_desktop_lid_close;
			CPRINTS("Thermal: Desktop lid close mode");
		}
	} else {
		thermal_params[TEMP_AMB] = thermal_laptop;
		thermal_params[TEMP_SOC] = fan_soc_laptop;
		thermal_params[TEMP_CPU] = fan_cpu_laptop;
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

#ifdef CONFIG_CUSTOM_FAN_DUTY_CONTROL
/**
 * Adjust fan duty by difference between target and actual rpm
 *
 * @param   ch        operation channel
 * @param   rpm_diff  difference between target and actual rpm
 * @param   duty      current fan duty
 */
static void fan_adjust_duty(int ch, int rpm_diff, int duty)
{
	int duty_step = 0;

	/* Find suitable duty step */
	if (ABS(rpm_diff) >= 1000) {
		duty_step = 10;
	} else if (ABS(rpm_diff) >= 500) {
		duty_step = 3;
	} else if (ABS(rpm_diff) >= 250) {
		duty_step = 2;
	} else if (ABS(rpm_diff) >= 100) {
		duty_step = 1;
	}

	/* Adjust fan duty step by step */
	if (rpm_diff > 0) {
		duty = MIN(duty + duty_step, 100);
	} else {
		duty = MAX(duty - duty_step, 1);
	}

	fan_set_duty(ch, duty);

	CPRINTS("fan%d: duty %d, rpm_diff %d", ch, duty, rpm_diff);
}

enum fan_status board_override_fan_control_duty(int ch)
{
	struct fan_data *data = &fan_data[ch];
	static int change_cnt, wait_cnt;
	bool change = false;
	int duty, rpm_diff;
	int rpm_actual = data->rpm_actual;
	int rpm_target = data->rpm_target;
	int deviation = fans[ch].rpm->rpm_deviation;

	CPRINTS("rpm_target: %d, rpm_actual: %d", rpm_target, rpm_actual);
	/* wait rpm is stable */
	if (ABS(rpm_actual - data->rpm_pre) > (rpm_target * deviation / 100)) {
		data->rpm_pre = rpm_actual;
		CPRINTS("Waiting...");
		return FAN_STATUS_CHANGING;
	}

	/* Record previous rpm */
	data->rpm_pre = rpm_actual;

	/* Adjust PWM duty */
	rpm_diff = rpm_target - rpm_actual;
	duty = fan_get_duty(ch);
	if (duty == 0 && rpm_target == 0) {
		return FAN_STATUS_STOPPED;
	}
	if (rpm_diff > (rpm_target / 100)) {
		/* Increase PWM duty */
		if (duty == 100) {
			return FAN_STATUS_FRUSTRATED;
		}
		change = true;
	} else if (rpm_diff < -(rpm_target / 100)) {
		/* Decrease PWM duty */
		if (duty == 1 && rpm_target != 0) {
			return FAN_STATUS_FRUSTRATED;
		}
		change = true;
	}
	if (change) {
		/* Do not make rapid changes in period > 4*tick =  800ms */
		if (change_cnt < 4) {
			fan_adjust_duty(ch, rpm_diff, duty);
			change_cnt++;
		} else {
			wait_cnt++;
			if (wait_cnt > 4) {
				wait_cnt = 0;
				change_cnt = 0;
			}
		}
		return FAN_STATUS_CHANGING;
	}

	wait_cnt = 0;
	change_cnt = 0;
	CPRINTS("Done!");
	return FAN_STATUS_LOCKED;
}
#endif
