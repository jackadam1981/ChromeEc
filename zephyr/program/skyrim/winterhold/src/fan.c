/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "fan.h"
#include "math_util.h"
#include "timer.h"
#include "thermal.h"
#include "util.h"

#include <zephyr/kernel.h>

#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_THERMAL, format, ##args)

struct pid_state {
	/* To calibrate the coefficients in runtime keep them as non-const. */
	float kp, ki, kd;

	float integral;
	float prev_error;
};

struct pid_state state = {
	.kp = 0.013000,
	.ki = 0.000030,
	.kd = 0.700000,

	.integral = 0,
	.prev_error = 0
};

float pid(struct pid_state *state, float error, float dt)
{
	float output;

	state->integral += dt * error;

	output = error * state->kp;
	output += state->integral * state->ki;
	output += (error - state->prev_error) * state->kd / dt;

	state->prev_error = error;

	return output;
}

void pid_reset_state(struct pid_state *state)
{
	state->integral = 0;
	state->prev_error = 0;
}

enum fan_status board_override_fan_control_duty(int ch)
{
	struct fan_data *data = &fan_data[ch];
	int rpm_actual = data->rpm_actual;
	int rpm_target = data->rpm_target;
	static timestamp_t last_tick;
	float new_duty, dt;
	int old_duty;
	/* In final solution this will be read from DT. */
	const float min_duty = 20, max_duty = 100;

	old_duty = fan_get_duty(ch);
	if (old_duty == 0 && rpm_target == 0) {
		pid_reset_state(&state);
		return FAN_STATUS_STOPPED;
	}

	/* If the fan was stopped for a while set dt to tick rate. */
	if (data->auto_status == FAN_STATUS_STOPPED)
		dt = 200;
	else
		dt = time_since32(last_tick) / 1000;

	new_duty = pid(&state, rpm_target - rpm_actual, dt);
	new_duty = CLAMP(new_duty, min_duty, max_duty);

	CPRINTS("rpm_target: %d, rpm_actual: %d, rpm_diff: %d, old_duty %d: new_duty: %d",
		rpm_target, rpm_actual, rpm_target - rpm_actual, old_duty, (int)new_duty);

	if ((int)new_duty == old_duty) {
		last_tick = get_time();
		return FAN_STATUS_LOCKED;
	}

	fan_set_duty(ch, (int)new_duty);

	last_tick = get_time();
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
