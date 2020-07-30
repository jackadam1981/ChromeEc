/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "body_detection.h"
#include "console.h"
#include "hwtimer.h"
#include "lid_switch.h"
#include "motion_sense_fifo.h"
#include "math_util.h"
#include "timer.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

static struct motion_sensor_t *sensor =
	&motion_sensors[CONFIG_BODY_DETECTION_SENSOR];

static int window_size = CONFIG_BODY_DETECTION_MAX_WINDOW_SIZE;
static uint64_t var_threshold_scaled, confidence_delta_scaled;
static int stationary_timeframe;

static int history_idx;
static enum body_detect_states motion_state = BODY_DETECTION_OFF_BODY;

static int history_initialized;
static int body_detect_enable = 1;

static struct body_detect_motion_data
{
	int history[CONFIG_BODY_DETECTION_MAX_WINDOW_SIZE]; /* acceleration */
	int sum;              /* sum(history) */
	uint64_t n2_variance; /* n^2 * var(history) */
} data[2]; /* motion data for X-axis and Y-axis */

/*
 * This function will update new variance and new sum according to incoming
 * value, previous value, previous sum and previous variance.
 * In order to prevent inaccuracy, we use integer to calculate instead of float
 *
 * n: window size
 * X: data in the old window
 * X': data in the new window
 * X0: oldest value in the window, will be replaced by Xn
 * Xn: new coming value
 *
 * n^2 * var(X') = n^2 * var(X) + (sum(X') - sum(X))^2 +
 *                 (n * Xn - sum(X'))^2 / n - (n * X0 - sum(X'))^2 / n
 */
static void update_motion_data(struct body_detect_motion_data *X, int Xn)
{
	const int n = window_size;
	const int X0 = X->history[history_idx];
	const int new_sum = X->sum + (Xn - X->history[history_idx]);

	X->n2_variance = X->n2_variance + SQ((int64_t)new_sum - X->sum) +
			 SQ((int64_t)Xn * n - new_sum) / n -
			 SQ((int64_t)X0 * n - new_sum) / n;
	X->sum = new_sum;
	X->history[history_idx] = Xn;
}

/* Update motion data of X, Y with new sensor data. */
static void update_motion_variance(void)
{
	int x, y;

	x = sensor->xyz[X];
	y = sensor->xyz[Y];

	update_motion_data(&data[X], x);
	update_motion_data(&data[Y], y);

	history_idx = (history_idx + 1 >= window_size) ? 0 : history_idx + 1;
}

/* return Var(X) + Var(Y) */
static uint64_t get_motion_variance(void)
{
	return (data[X].n2_variance + data[Y].n2_variance)
		/ window_size / window_size;
}

static int calculate_motion_confidence(uint64_t var)
{
	if (var < var_threshold_scaled - confidence_delta_scaled)
		return 0;
	if (var > var_threshold_scaled + confidence_delta_scaled)
		return 100;
	return 100 * (var - var_threshold_scaled + confidence_delta_scaled) /
		(2 * confidence_delta_scaled);
}

static void debug_log(void)
{
	uint64_t motion_var = get_motion_variance();
	int motion_confidence = calculate_motion_confidence(motion_var);

#ifndef TEST_BUILD
	CPRINTS("[%s body] variance: %llu (%d%%)",
		motion_state ? "on" : "off", motion_var, motion_confidence);
#else
	CPRINTS("[%s body] variance: %lu (%d%%)",
		motion_state ? "on" : "off", motion_var, motion_confidence);
#endif
}

/* Change the motion state and commit the change to AP. */
void body_detect_change_state(enum body_detect_states state)
{
#ifdef CONFIG_GESTURE_HOST_DETECTION
	struct ec_response_motion_sensor_data vector = {
		.flags = MOTIONSENSE_SENSOR_FLAG_WAKEUP,
		.activity = MOTIONSENSE_ACTIVITY_BODY_DETECTION,
		.state = state,
		.sensor_num = MOTION_SENSE_ACTIVITY_SENSOR_ID,
	};
	motion_sense_fifo_stage_data(&vector, NULL, 0,
			__hw_clock_source_read());
	motion_sense_fifo_commit_data();
#endif
	/* change the motion state */
	motion_state = state;
	if (state == BODY_DETECTION_ON_BODY) {
		/* reset time counting of stationary */
		stationary_timeframe = 0;
	}
	/* state changing log */
	debug_log();
}

enum body_detect_states body_detect_get_state(void)
{
	return motion_state;
}

/* Determine window size for 1 second by sensor data rate. */
static void determine_window_size(int odr)
{
	window_size = odr / 1000;
	/* Normally, window_size should not exceed MAX_WINDOW_SIZE. */
	if (window_size > CONFIG_BODY_DETECTION_MAX_WINDOW_SIZE) {
		/* This will cause window size not enough for 1 second */
		CPRINTS("ODR exceeds CONFIG_BODY_DETECTION_MAX_WINDOW_SIZE");
		window_size = CONFIG_BODY_DETECTION_MAX_WINDOW_SIZE;
	}
}

/* Determine variance threshold scale by range and resolution. */
static void determine_threshold_scale(int range, int resolution, int rms_noise)
{
	/*
	 * range:              g
	 * resolution:         bits
	 * data_1g:            LSB/g
	 * data_1g / 9800:     LSB/(mm/s^2)
	 * (data_1g / 9800)^2: (LSB^2)/(mm^2/s^4), which number of
	 *                     var(sensor data) will represents 1 (mm^2/s^4)
	 * rms_noise:          ug
	 * var_noise:          mm^2/s^4
	 */
	const int data_1g = BIT(resolution - 1) / range;
	const int multiplier = SQ(data_1g);
	const int divisor = SQ(9800);
	const int var_noise = SQ((int64_t)rms_noise) * SQ(98) / SQ(10000);

	var_threshold_scaled = (uint64_t)
		(CONFIG_BODY_DETECTION_VAR_THRESHOLD + var_noise) *
		multiplier / divisor;
	confidence_delta_scaled = (uint64_t)
		CONFIG_BODY_DETECTION_CONFIDENCE_DELTA *
		multiplier / divisor;
}

void body_detect_reset(void)
{
	int odr = sensor->drv->get_data_rate(sensor);
	int range = sensor->drv->get_range(sensor);
	int resolution = sensor->drv->get_resolution(sensor);
	int rms_noise = sensor->drv->get_rms_noise(sensor);
	/*
	 * The sensor is suspended since its ODR is 0,
	 * there is no need to reset until sensor is up again
	 */
	if (odr == 0)
		return;
	determine_window_size(odr);
	determine_threshold_scale(range, resolution, rms_noise);
	/* initialize motion data and state */
	memset(data, 0, sizeof(data));
	history_idx = 0;
	history_initialized = 0;
	body_detect_change_state(BODY_DETECTION_ON_BODY);
}

int body_detect(void)
{
	uint64_t motion_var;
	int motion_confidence;

	if (!body_detect_enable)
		return EC_SUCCESS;

	update_motion_variance();
	if (!history_initialized) {
		if (history_idx == window_size - 1)
			history_initialized = 1;
		return EC_SUCCESS;
	}

	motion_var = get_motion_variance();
	motion_confidence = calculate_motion_confidence(motion_var);
	switch (motion_state) {
	case BODY_DETECTION_OFF_BODY:
		if (motion_confidence > CONFIG_BODY_DETECTION_ON_BODY_CON)
			body_detect_change_state(BODY_DETECTION_ON_BODY);
		break;
	case BODY_DETECTION_ON_BODY:
		stationary_timeframe += 1;
		/* confidence exceeds the limit, reset time counting */
		if (motion_confidence >= CONFIG_BODY_DETECTION_OFF_BODY_CON)
			stationary_timeframe = 0;
		/* if no motion for enough time, change state to off_body */
		if (stationary_timeframe >=
		    CONFIG_BODY_DETECTION_STATIONARY_DURATION * window_size)
			body_detect_change_state(BODY_DETECTION_OFF_BODY);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	if (history_idx == 0)
		debug_log();
	return EC_SUCCESS;
}

void body_detect_set_enable(int enable)
{
	body_detect_enable = enable;
	body_detect_change_state(BODY_DETECTION_ON_BODY);
}

int body_detect_get_enable(void)
{
	return body_detect_enable;
}
