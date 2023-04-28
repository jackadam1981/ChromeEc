/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "fan.h"
#include "math_util.h"
#include "thermal.h"
#include "util.h"

#include <zephyr/kernel.h>

#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_THERMAL, format, ##args)

struct pid_state {
	float kp;
	float ki;
	float kd;

	/*
	 * Ideally we'd use double here.
	 * The problem is that some chips support single-precision floating
	 * point only. Using doubles on them results in:
	 * 'undefined reference to `__aeabi_f2d'' linker error.
	 */
	float integral;
	float prev_error;
};

struct pid_state state = {
	.kp = 0.0400,
	.ki = 0.0070,
	.kd = 0.0005,

	.integral = 0,
	.prev_error = 0
};

float pid(struct pid_state *state, float error)
{
	float output;

	state->integral += error;

	output = error * state->kp;		/* P */
	output += state->integral * state->ki;	/* I */
	output += state->prev_error * state->kd;/* D */

	state->prev_error = error;

	return output;
}

enum fan_status board_override_fan_control_duty(int ch)
{
	struct fan_data *data = &fan_data[ch];
	int rpm_actual = data->rpm_actual;
	int rpm_target = data->rpm_target;
	int old_duty;
	float new_duty;

	old_duty = fan_get_duty(ch);
	if (old_duty == 0 && rpm_target == 0) {
		state.integral = 0;
		state.prev_error = 0;
		return FAN_STATUS_STOPPED;
	}

	new_duty = pid(&state, rpm_target - rpm_actual);

	if (new_duty > 0.0f)
		new_duty = MIN(new_duty, 100.0f);
	else
		new_duty = 0.0f;

	if ((int)new_duty == old_duty)
		return FAN_STATUS_LOCKED;

	fan_set_duty(ch, (int)new_duty);

	CPRINTS("rpm_target: %d, rpm_actual: %d, rpm_diff: %d, old_duty %d: new_duty: %d",
		rpm_target, rpm_actual, rpm_target - rpm_actual, old_duty, (int)new_duty);

	return FAN_STATUS_CHANGING;
}

static int command_setpid(int argc, const char **argv)
{
	float val;

	if (argc == 1) {
		CPRINTS("kp: %d, ki: %d, kd: %d",
			(int)(state.kp * 1000000),
			(int)(state.ki * 1000000),
			(int)(state.kd * 1000000));
		return EC_SUCCESS;
	}

	if (argc != 4)
		return EC_ERROR_PARAM_COUNT;

	val = strtoi(argv[1], NULL, 0);
	state.kp = val / 1000000;

	val = strtoi(argv[2], NULL, 0);
	state.ki = val / 1000000;

	val = strtoi(argv[3], NULL, 0);
	state.kd = val / 1000000;

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(
	setpid, command_setpid, "kp, kd, ki",
	"Set the PID parameters."
	"Three integers are expected, each is shifted by 6 deciaml places.");
