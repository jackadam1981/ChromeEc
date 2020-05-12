/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* add comment */


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

#define SQ(x) ((x) * (x))

enum body_states {
	OFF_BODY,
	ON_BODY
};

static struct motion_sensor_t *sensor =
	&motion_sensors[CONFIG_BODY_DETECTION_SENSOR];

static int window_size;
static int sensor_data_interval;
static uint64_t var_threshold_scaled, confidence_delta_scaled;
static timestamp_t ts_stationary;
static uint32_t next_collection;

static int history_idx;
static int motion_state = OFF_BODY;

static struct body_detection_motion_data
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
static void update_new_variance(struct body_detection_motion_data *X, int Xn)
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

/*
 * Update motion data of X, Y with new x, y value.
 * return var(X) + var(Y)
 */
static uint64_t get_motion_variance(void)
{
	int x, y;

	x = sensor->xyz[X];
	y = sensor->xyz[Y];

	update_new_variance(&data[X], x);
	update_new_variance(&data[Y], y);

	history_idx = (history_idx + 1 >= window_size) ? 0 : history_idx + 1;

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
	uint64_t motion_var = (data[X].n2_variance + data[Y].n2_variance) /
			      window_size / window_size;
	int motion_confidence = calculate_motion_confidence(motion_var);

	CPRINTS("[%s body] variance: %lld (%d%%)",
		 motion_state ? "on" : "off", motion_var, motion_confidence);
}

/* Change the motion state and commit the change to AP. */
static void change_state(int state)
{
	/* TODO(chingkang): test if AP can handle the data */
	struct ec_response_motion_sensor_data vector = {
		.flags = MOTIONSENSE_SENSOR_FLAG_WAKEUP,
		.activity = MOTIONSENSE_ACTIVITY_BODY_DETECTION,
		.state = state,
		.sensor_num = CONFIG_BODY_DETECTION_SENSOR,
	};
	motion_sense_fifo_stage_data(&vector, NULL, 0,
			__hw_clock_source_read());
	motion_sense_fifo_commit_data();

	/* change the motion state */
	motion_state = state;
	if (state == ON_BODY)
		ts_stationary = get_time();
	/* state chage log */
	debug_log();
}

/* Determine window size for 1sec by sensor data rate. */
static void determine_window_size(int odr)
{
	/* window_size should not exceed MAX_WINDOW_SIZE */
	window_size = MIN(CONFIG_BODY_DETECTION_MAX_WINDOW_SIZE, odr / 1000);
	sensor_data_interval = SECOND / window_size;
}

/* Determine variance threshold scale by range and resolution. */
static void determine_threshold_scale(int range, int resolution)
{
	/*
	 * range:              g
	 * resolution:         bits
	 * data_1g:            LSB/g
	 * data_1g / 9800:     LSB/(mm/s^2)
	 * (data_1g / 9800)^2: (LSB^2)/(mm^2/s^4), how much is var(sensor data)
	 *                     that represents 1 (mm^2/s^4)
	 */
	int data_1g = (1 << (resolution - 1)) / range;

	var_threshold_scaled =
		(uint64_t)CONFIG_BODY_DETECTION_VAR_THRESHOLD *
		data_1g * data_1g / 96040000;
	confidence_delta_scaled =
		(uint64_t)CONFIG_BODY_DETECTION_CONFIDENCE_DELTA *
		data_1g * data_1g / 96040000;
	CPRINTS("[body detection] var_threshold: %lld, con_delta: %lld",
		var_threshold_scaled, confidence_delta_scaled);
}

/* This will be called after booting or body_detection_reset() is called. */
static int body_detection_init(void)
{
	static int last_odr, last_range, last_resolution;
	int odr = sensor->drv->get_data_rate(sensor);
	int range = sensor->drv->get_range(sensor);
	int resolution = sensor->drv->get_resolution(sensor);

	/* invalid odr, range or resolution */
	if (odr <= 0 || range <= 0 || resolution <= 0)
		return EC_ERROR_TRY_AGAIN;
	/* values is not changed, no need to init */
	if (odr == last_odr && last_range == range &&
	    last_resolution == resolution)
		return EC_SUCCESS;
	last_odr = odr;
	last_range = range;
	last_resolution = resolution;

	determine_window_size(odr);
	determine_threshold_scale(range, resolution);
	/* initialize motion data and state */
	memset(data, 0, sizeof(data));
	history_idx = 0;
	change_state(ON_BODY);
	next_collection = get_time().le.lo;
	return EC_SUCCESS;
}

int body_detect(void)
{
	uint64_t motion_var;
	int motion_confidence;
	timestamp_t ts_current = get_time();

	if (body_detection_init() != EC_SUCCESS)
		return EC_ERROR_TRY_AGAIN;

	/* make sure that the sampling rate is correct */
	if (!time_after(ts_current.le.lo, next_collection))
		return EC_ERROR_TRY_AGAIN;
	next_collection += sensor_data_interval;

	motion_var = get_motion_variance();
	motion_confidence = calculate_motion_confidence(motion_var);

	switch (motion_state) {
	case OFF_BODY:
		if (motion_confidence > CONFIG_BODY_DETECTION_ON_BODY_CON)
			change_state(ON_BODY);
		break;
	case ON_BODY:
		if (motion_confidence >= CONFIG_BODY_DETECTION_OFF_BODY_CON) {
			/* confidence exceeds the limit, reset time counting */
			ts_stationary = ts_current;
		}
		/* if no motion for enough time, change state to off_body */
		if (time_after(ts_current.le.lo, ts_stationary.le.lo +
			       CONFIG_BODY_DETECTION_STATIONARY_DURATION))
			change_state(OFF_BODY);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}


