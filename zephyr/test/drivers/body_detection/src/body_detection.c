/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "body_detection.h"
#include "console.h"
#include "data.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(int, get_data_rate, struct motion_sensor_t *);
FAKE_VALUE_FUNC(int, get_rms_noise, struct motion_sensor_t *);

extern struct motion_sensor_t *body_sensor;
extern float var_threshold;
extern float confidence_delta;

/*
 * In order to be independent from a motion sensor driver changes mock
 * two functions that are used during body detection parameters initialization.
 */
const static struct accelgyro_drv mock_drv = {
	.get_data_rate = get_data_rate,
	.get_rms_noise = get_rms_noise,
};
const static struct accelgyro_drv *old_drv;

static void body_detect_before(void *state)
{
	ARG_UNUSED(state);
	body_detect_reset();
}

static void body_detect_after(void *state)
{
	ARG_UNUSED(state);
	body_detect_reset();
}

/**
 * @brief TestPurpose: various body_detect_change_state operations.
 */
ZTEST_USER(bodydetectmode, test_body_detect_set_state)
{
	enum body_detect_states body_detect_state;

	body_detect_state = body_detect_get_state();
	zassert_equal(body_detect_state, BODY_DETECTION_ON_BODY,
		      "unexpected body detect initial mode: %d",
		      body_detect_state);

	body_detect_change_state(BODY_DETECTION_OFF_BODY, false);
	body_detect_state = body_detect_get_state();
	zassert_equal(body_detect_state, BODY_DETECTION_OFF_BODY,
		      "unexpected body detect mode: %d", body_detect_state);

	body_detect_change_state(BODY_DETECTION_ON_BODY, false);
	body_detect_state = body_detect_get_state();
	zassert_equal(body_detect_state, BODY_DETECTION_ON_BODY,
		      "unexpected body detect mode: %d", body_detect_state);
}

/**
 * @brief TestPurpose: ensure that console bodydetectmode forces the status,
 * inhibiting body_detect_change_state, and then unforce it with reset.
 */
ZTEST_USER(bodydetectmode, test_setbodydetectionmode_forced)
{
	int ret;
	enum body_detect_states body_detect_state;

	body_detect_state = body_detect_get_state();
	zassert_equal(body_detect_state, BODY_DETECTION_ON_BODY,
		      "unexpected body detect initial mode: %d",
		      body_detect_state);

	/**
	 * Set body detect mode to "off", since it defaults "on".
	 */
	ret = shell_execute_cmd(get_ec_shell(), "bodydetectmode off");
	zassert_equal(ret, EC_SUCCESS, "unexpected command return status: %d",
		      ret);
	body_detect_state = body_detect_get_state();
	zassert_equal(body_detect_state, BODY_DETECTION_OFF_BODY,
		      "unexpected body detect mode: %d", body_detect_state);

	/**
	 * Set body detect mode to "on", to validate it can be enabled also.
	 */
	ret = shell_execute_cmd(get_ec_shell(), "bodydetectmode on");
	zassert_equal(ret, EC_SUCCESS, "unexpected command return status: %d",
		      ret);
	body_detect_state = body_detect_get_state();
	zassert_equal(body_detect_state, BODY_DETECTION_ON_BODY,
		      "unexpected body detect mode: %d", body_detect_state);

	/**
	 * Set body detect mode to "off", since it defaults "on".
	 */
	ret = shell_execute_cmd(get_ec_shell(), "bodydetectmode off");
	zassert_equal(ret, EC_SUCCESS, "unexpected command return status: %d",
		      ret);
	body_detect_state = body_detect_get_state();
	zassert_equal(body_detect_state, BODY_DETECTION_OFF_BODY,
		      "unexpected body detect mode: %d", body_detect_state);

	/**
	 * Reset body detect mode. This returns body detect to "on".
	 */
	ret = shell_execute_cmd(get_ec_shell(), "bodydetectmode reset");
	zassert_equal(ret, EC_SUCCESS, "unexpected command return status: %d",
		      ret);
	body_detect_state = body_detect_get_state();
	zassert_equal(body_detect_state, BODY_DETECTION_ON_BODY,
		      "unexpected body detect mode: %d", body_detect_state);
}

/**
 * @brief TestPurpose: check the "too many arguments" case.
 */
ZTEST_USER(bodydetectmode, test_setbodydetectionmode_too_many_args)
{
	int ret;

	ret = shell_execute_cmd(get_ec_shell(),
				"bodydetectmode too many arguments");
	zassert_equal(ret, EC_ERROR_PARAM_COUNT,
		      "unexpected command return status: %d", ret);
}

/**
 * @brief TestPurpose: check the "unknown argument" case.
 */
ZTEST_USER(bodydetectmode, test_setbodydetectionmode_unknown_arg)
{
	int ret;

	ret = shell_execute_cmd(get_ec_shell(), "bodydetectmode X");
	zassert_equal(ret, EC_ERROR_PARAM1,
		      "unexpected command return status: %d", ret);
}

ZTEST_SUITE(bodydetectmode, drivers_predicate_post_main, NULL,
	    body_detect_before, body_detect_after, NULL);

static void body_detect_init_before(void *state)
{
	ARG_UNUSED(state);

	RESET_FAKE(get_data_rate);
	RESET_FAKE(get_rms_noise);

	/* ODR = 50 Hz */
	get_data_rate_fake.return_val = 50 * 1000;
	/* RMS noise of LIS2DW12 with ODR set to 50Hz */
	get_rms_noise_fake.return_val = 636;

	old_drv = body_sensor->drv;
	body_sensor->drv = &mock_drv;
	body_detect_reset();
}

static void body_detect_init_after(void *state)
{
	ARG_UNUSED(state);

	body_sensor->drv = old_drv;
	body_sensor->bd_params = NULL;
	body_detect_reset();
}

#define DEFAULT_CONFIDENCE_DELTA CONFIG_BODY_DETECTION_CONFIDENCE_DELTA
#define DEFAULT_VAR_THRESHOLD CONFIG_BODY_DETECTION_VAR_THRESHOLD

/**
 * @brief TestPurpose: check variance properties with default input parameters
 */
ZTEST_USER(bodydetectinit, test_defaultparams)
{
	/*
	 * body_detect_reset was already called in body_detect_init_before.
	 * No need to invoke it here.
	 */
	zassert_equal(confidence_delta, DEFAULT_CONFIDENCE_DELTA);
	zassert_equal(var_threshold, DEFAULT_VAR_THRESHOLD);
	zassert_equal(1, get_data_rate_fake.call_count);
}

/**
 * @brief TestPurpose: check variance properties with custom parameters
 * If any parameter is set to zero it should be replaced with default
 * value read from Kconfig.
 */
ZTEST_USER(bodydetectinit, test_customparams)
{
	struct body_detect_params params;

	body_sensor->bd_params = &params;

	memset(&params, 0, sizeof(params));

	body_detect_reset();
	zassert_equal(confidence_delta, DEFAULT_CONFIDENCE_DELTA);
	zassert_equal(var_threshold, DEFAULT_VAR_THRESHOLD);
	zassert_equal(2, get_data_rate_fake.call_count);

	params.confidence_delta = 2900;
	params.var_threshold = 3000;

	body_detect_reset();
	zassert_equal(confidence_delta, 2900);
	zassert_equal(var_threshold, 3000);
	zassert_equal(3, get_data_rate_fake.call_count);
}

ZTEST_SUITE(bodydetectinit, drivers_predicate_post_main, NULL,
	    body_detect_init_before, body_detect_init_after, NULL);

void body_detect_step(float x, float y, float z, uint64_t curtime);
extern timestamp_t (*get_time_ptr)();
static timestamp_t (*get_time_ptr_old)();
static uint64_t time_fake_value;
static timestamp_t get_time_fake(void)
{
	timestamp_t time;

	time.val = time_fake_value;

	return time;
}

static void body_detect_sample_before(void *state)
{
	struct body_detect_params params;

	ARG_UNUSED(state);
	get_time_ptr_old = get_time_ptr;
	get_time_ptr = get_time_fake;

	old_drv = body_sensor->drv;
	body_sensor->drv = &mock_drv;

	RESET_FAKE(get_data_rate);
	RESET_FAKE(get_rms_noise);

	/* ODR = 50 Hz */
	get_data_rate_fake.return_val = 50 * 1000;
	/* RMS noise of LIS2DW12 with ODR set to 50Hz */
	get_rms_noise_fake.return_val = 636;

	body_sensor->bd_params = &params;
	memset(&params, 0, sizeof(params));

	params.confidence_delta = 900;
	params.var_threshold = 1000;

	body_detect_reset();
	zassert_equal(confidence_delta, 900);
	zassert_equal(var_threshold, 1000);
}

static void body_detect_sample_after(void *state)
{
	ARG_UNUSED(state);
	get_time_ptr = get_time_ptr_old;
	body_sensor->bd_params = NULL;
	body_sensor->drv = old_drv;

	body_detect_reset();
}

/*
 * Standarized test points.
 * Scenarios description: go/bd-overhaul
 */
struct sample_mock_test_vector {
	int time_offset; /* Time offset in seconds */
	int onbody; /* Expected state */
};
static struct sample_mock_test_vector vectors[] = {
	/* Steady state, DUT on the desk */
	{ .time_offset = 10, .onbody = 0 },
	/* DUT on desk, browsing internet */
	{ .time_offset = 250, .onbody = 0 },
	/* DUT on desk, typing */
	{ .time_offset = 400, .onbody = 0 },
	/* DUT on lap, browsing internet */
	{ .time_offset = 610, .onbody = 1 },
	/* DUT on lap, browsing internet */
	{ .time_offset = 710, .onbody = 1 },
	/* DUT on lap, watching Youtube */
	{ .time_offset = 730, .onbody = 1 },
	/* DUT on lap, watching Youtube */
	{ .time_offset = 800, .onbody = 1 },
	/* DUT on chest, watching Youtube */
	{ .time_offset = 1100, .onbody = 1 },
	/* DUT on chest, watching Youtube */
	{ .time_offset = 1200, .onbody = 1 },
	/* DUT on chest, watching Youtube */
	{ .time_offset = 1500, .onbody = 1 },
	/* end */
	{ .time_offset = 0, 0 }
};

/**
 * @brief TestPurpose: provide real-life data to feed the algorithm.
 */
ZTEST_USER(bodydetectsample, test_setbodydetectionmode_unknown_arg)
{
	int i, cnt;
	struct sample_mock_test_vector *vector = vectors;

	/*
	 * Run body_detect for each RAW sample.
	 */
	cnt = sizeof(accel_data_mock) / sizeof(struct accel_data);
	for (i = 0; i < cnt; i++) {
		time_fake_value = (accel_data_mock[i].timestamp -
				   accel_data_mock[0].timestamp) *
				  1000000.0f;
		body_sensor->xyz[X] = accel_data_mock[i].x;
		body_sensor->xyz[Y] = accel_data_mock[i].y;
		body_sensor->xyz[Z] = accel_data_mock[i].z;
		body_detect();

		/* Test if on/off body state is valid */
		if (vector->time_offset != 0 &&
		    (vector->time_offset <= (time_fake_value / 1000000))) {
			zassert_equal(vector->onbody, body_detect_get_state(),
				      "expected state %d at %d[s]",
				      vector->onbody, vector->time_offset);
			vector++;
		}
	}
}

ZTEST_SUITE(bodydetectsample, drivers_predicate_post_main, NULL,
	    body_detect_sample_before, body_detect_sample_after, NULL);

ZTEST_USER(bodydetectonoff, test_setenable)
{
	enum body_detect_states body_detect_state;
	int enabled;

	body_detect_state = body_detect_get_state();
	zassert_equal(body_detect_state, BODY_DETECTION_ON_BODY,
		      "unexpected body detect initial mode: %d",
		      body_detect_state);

	body_detect_set_enable(0);
	enabled = body_detect_get_enable();
	zassert_equal(enabled, 0, "unexpected state, enabled = %d", enabled);

	body_detect_set_enable(1);
	enabled = body_detect_get_enable();
	zassert_equal(enabled, 1, "unexpected state, enabled = %d", enabled);
}

ZTEST_SUITE(bodydetectonoff, drivers_predicate_post_main, NULL,
	    body_detect_before, body_detect_after, NULL);
