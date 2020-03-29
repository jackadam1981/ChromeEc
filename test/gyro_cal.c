/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "gyro_cal.h"
#include "gyro_still_det.h"
#include "motion_sense.h"
#include "test_util.h"
#include <string.h>

float kToleranceGyroRps = 1e-6f;
float kDefaultGravityMps2 = 9.81f;
float kDefaultTemperatureCelsius = 25.0f;

#define DEG_TO_RAD (3.14159265f / 180.0f)

struct motion_sensor_t motion_sensors[] = {
	[BASE] = {},
	[LID] = {},
};

const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

/**
 *
 * @param det              Pointer to the stillness detector
 * @param var_threshold    The variance threshold in units^2
 * @param confidence_delta The confidence delta in units^2
 */
void gyro_still_det_initialization(
	struct gyro_still_det* det, float var_threshold, float confidence_delta)
{
	/* Clear all data structure variables to 0. */
	memset(det, 0, sizeof (struct gyro_still_det));

	/*
	 * Set the delta about the variance threshold for calculation
	 * of the stillness confidence score.
	 */
	if (confidence_delta < var_threshold) {
		det->confidence_delta = confidence_delta;
	} else {
		det->confidence_delta = var_threshold;
	}

	/*
	 * Set the variance threshold parameter for the stillness
	 * confidence score.
	 */
	det->var_threshold = var_threshold;

	/* Signal to start capture of next stillness data window. */
	det->start_new_window = true;
}

void gyro_cal_initialization(struct gyro_cal* gyro_cal)
{
	/* GyroCal initialization (Sensing Device = BMI160). */
	memset(gyro_cal, 0, sizeof(struct gyro_cal));

	/*
	 * Initialize the stillness detectors.
	 * Gyro parameter input units are [rad/sec].
	 * Accel parameter input units are [m/sec^2].
	 * Magnetometer parameter input units are [uT].
	 */
	gyro_still_det_initialization(
		&gyro_cal->gyro_stillness_detect,
		/* var_threshold */ 5e-5f,
		/* confidence_delta */ 1e-5f);
	gyro_still_det_initialization(
		&gyro_cal->accel_stillness_detect,
		/* var_threshold */ 8e-3f,
		/* confidence_delta */ 1.6e-3f);
	gyro_still_det_initialization(
		&gyro_cal->mag_stillness_detect,
		/* var_threshold */ 1.4f,
		/* confidence_delta */ 0.25f);

	/* Reset stillness flag and start timestamp. */
	gyro_cal->prev_still = false;
	gyro_cal->start_still_time_us = 0;

	/* Set the min and max window stillness duration. */
	gyro_cal->min_still_duration_us = 5 * SECOND;
	gyro_cal->max_still_duration_us = 6 * SECOND;

	/* Sets the duration of the stillness processing windows. */
	gyro_cal->window_time_duration_us = 1500000;

	/* Set the watchdog timeout duration. */
	gyro_cal->gyro_watchdog_timeout_duration_us = 5 * SECOND;

	/* Load the last valid cal from system memory. */
	gyro_cal->bias_x = 0.0f;  /* [rad/sec] */
	gyro_cal->bias_y = 0.0f;  /* [rad/sec] */
	gyro_cal->bias_z = 0.0f;  /* [rad/sec] */
	gyro_cal->calibration_time_us = 0;

	/* Set the stillness threshold required for gyro bias calibration. */
	gyro_cal->stillness_threshold = 0.95f;

	/*
	 * Current window end-time used to assist in keeping sensor data
	 * collection in sync. Setting this to zero signals that sensor data
	 * will be dropped until a valid end-time is set from the first gyro
	 * timestamp received.
	 */
	gyro_cal->stillness_win_endtime_us = 0;

	/* Gyro calibrations will be applied (see, gyro_cal_remove_bias()). */
	gyro_cal->gyro_calibration_enable = true;

	/*
	 * Sets the stability limit for the stillness window mean acceptable
	 * delta.
	 */
	gyro_cal->stillness_mean_delta_limit = 50.0f * DEG_TO_RAD;

	/* Sets the min/max temperature delta limit for the stillness period. */
	gyro_cal->temperature_delta_limit_celsius = 1.5f;

	/* Ensures that the data tracking functionality is reset. */
	gyro_cal_reset(gyro_cal);
}

static int test_gyro_cal_calibration(void)
{
	struct gyro_cal gyro_cal;

	gyro_cal_initialization(&gyro_cal);

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_gyro_cal_calibration);

	test_print_result();
}