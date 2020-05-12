/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* add comment */

#include "on_body_detection.h"

#include "accelgyro.h"
#include "console.h"
#include "lid_switch.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

enum on_body_states {
	ON_BODY,
	WAITING_FOR_OFF_BODY,
	OFF_BODY
};

static struct motion_sensor_t *sensor =
&motion_sensors[BASE_ACCEL];

/* TODO(chingkang): make the window size for 1s always */
#define WINDOW_SIZE 50

/* TODO(chingkang): make the scale according to accel range */
/* change unit (m/s^2)^2 to (g/8192)^2 (unit in BMI160) */
const int raw_data_scale = 8192 * 8192 / 9.8 / 9.8;

const int variance_threshold = 0.0006 * raw_data_scale;
const int confidence_delta = 0.00055 * raw_data_scale;
const int on_body_confidence = 0.5 * 100;
const int off_body_confidence = 0.1 * 100;
const int stationary_threshold_duration = 15 * WINDOW_SIZE;

int calculate_motion_confidence(int variance)
{
	if (variance < variance_threshold - confidence_delta)
		return 0;
	if (variance > variance_threshold + confidence_delta)
		return 100;
	return 100 * (variance - variance_threshold + confidence_delta) /
	       (2 * confidence_delta);
}

static int history_x[WINDOW_SIZE] = {0};
static int history_y[WINDOW_SIZE] = {0};
static int history_idx;
static int initialized;
static int motion_state = OFF_BODY;

/* TODO(chingkang): implement this */
void motion_detection_init(void)
{
	initialized = 0;
	motion_state = OFF_BODY;
}

/*
 * This function will update new variance and new mean according to new
 * value, previous variance and previous mean.
 * n: window size
 * mean: mean of acceleration in the sliding window
 * mean': mean in the new sliding window
 * variance: variance of acceleration in the sliding window
 * variance': variance in the new sliding window
 * X_k: k-th value in the window
 * n^2 * variance' = n^2 * variance + (n * mean' - n * mean)^2 +
 *                   (n * X_n - n * mean')^2 / n - (n * X_0 - n * mean')^2 / n
 */
static void update_new_variance(int new_val, int64_t *n2_variance_p,
				int *n_mean_p, int *history)
{
	int new_n_mean = *n_mean_p + (new_val - history[history_idx]);
	int delta_n_mean = new_n_mean - *n_mean_p;
	int64_t norm_new_val = new_val * WINDOW_SIZE - new_n_mean;
	int64_t norm_old_val = history[history_idx] * WINDOW_SIZE - new_n_mean;


	*n2_variance_p = *n2_variance_p + (int64_t)delta_n_mean * delta_n_mean +
		   norm_new_val * norm_new_val / WINDOW_SIZE -
		   norm_old_val * norm_old_val / WINDOW_SIZE;

	*n_mean_p = new_n_mean;
	history[history_idx] = new_val;
}

/* return var_X + var_Y */
static int new_variance_x_plus_y(void)
{
	int x, y;
	static int64_t n2_variance_x, n2_variance_y;
	static int n_mean_x, n_mean_y;

	x = sensor->xyz[0];
	y = sensor->xyz[1];

	update_new_variance(x, &n2_variance_x, &n_mean_x, history_x);
	update_new_variance(y, &n2_variance_y, &n_mean_y, history_y);
	if (history_idx == WINDOW_SIZE)
		history_idx = 0;
	history_idx = (history_idx == WINDOW_SIZE - 1) ? 0 : history_idx + 1;

	return (n2_variance_x + n2_variance_y) / WINDOW_SIZE / WINDOW_SIZE;
}

static void debug_print(int motion_state, int motion_var)
{
	char *debug_message = "";
	/* debug message */
	if (motion_state == ON_BODY)
		debug_message = "in motion";
	else if (motion_state == OFF_BODY)
		debug_message = "stationary";
	else
		debug_message = "waiting for off body";

	if (history_idx % 5 == 0)
		CPRINTS("[%s] x: %d, y: %d, var_X + var_Y: %d",
			debug_message,
			sensor->xyz[0], sensor->xyz[1],
			motion_var);
}

static void motion_cal(void)
{
	static int off_body_cnt;
	int motion_var = new_variance_x_plus_y();
	int motion_confidence = calculate_motion_confidence(motion_var);

	switch (motion_state) {
	case OFF_BODY:
		if (motion_confidence > on_body_confidence)
			motion_state = ON_BODY;
		break;
	case ON_BODY:
		if (motion_confidence < off_body_confidence)
			motion_state = WAITING_FOR_OFF_BODY;
		break;
	case WAITING_FOR_OFF_BODY:
		off_body_cnt += 1;
		if (motion_confidence >= off_body_confidence) {
			off_body_cnt = 0;
			motion_state = ON_BODY;
		}
		/* if no motion for 15s, change state to off_body */
		if (off_body_cnt >= stationary_threshold_duration) {
			off_body_cnt = 0;
			motion_state = OFF_BODY;
		}
		break;
	default:
		CPRINTS("unknown motion state");
	}

	debug_print(motion_state, motion_var);
}


void motion_detection(void)
{
	motion_cal();
}

