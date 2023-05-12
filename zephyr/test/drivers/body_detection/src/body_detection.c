/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "body_detection.h"
#include "console.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

extern struct motion_sensor_t *body_sensor;
extern uint64_t var_threshold_scaled, confidence_delta_scaled;

static void body_detect_mode_before(void *state)
{
	ARG_UNUSED(state);
	body_detect_reset();
}

static void body_detect_mode_after(void *state)
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
	    body_detect_mode_before, body_detect_mode_after, NULL);

/*
 * In order to be independent from a motion sensor driver changes mock
 * two functions that are used during body detection parameters initialization.
 */
int get_rms_noise_mock(struct motion_sensor_t *sensor)
{
	/* RMS noise from LIS12DW with ODR set to 50Hz */
	return 636;
}
int get_data_rate_mock(struct motion_sensor_t *sensor)
{
	return 50 * 1000; /* 50 Hz */
}

const static struct accelgyro_drv mock_drv = {
	.get_data_rate = get_data_rate_mock,
	.get_rms_noise = get_rms_noise_mock
};

const static struct accelgyro_drv *old_drv;

static void body_detect_init_before(void *state)
{
	ARG_UNUSED(state);

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

#define DEFAULT_CONFIDENCE_DELTA 1467
#define DEFAULT_VAR_THRESHOLD 1665

/**
 * @brief TestPurpose: check variance properties with default input parameters
 */
ZTEST_USER(bodydetectinit, test_defaultparams)
{
	zassert_equal(confidence_delta_scaled, DEFAULT_CONFIDENCE_DELTA);
	zassert_equal(var_threshold_scaled, DEFAULT_VAR_THRESHOLD);
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
	zassert_equal(confidence_delta_scaled, DEFAULT_CONFIDENCE_DELTA);
	zassert_equal(var_threshold_scaled, DEFAULT_VAR_THRESHOLD);

	params.confidence_delta = 2900;
	params.var_threshold = 3000;

	body_detect_reset();
	zassert_equal(confidence_delta_scaled, 8105);
	zassert_equal(var_threshold_scaled, 8513);

	params.confidence_delta = 2900;
	params.var_threshold = 3000;
	params.var_noise_factor = 150;

	body_detect_reset();
	zassert_equal(confidence_delta_scaled, 8105);
	zassert_equal(var_threshold_scaled, 8547);
}

ZTEST_SUITE(bodydetectinit, drivers_predicate_post_main, NULL,
	    body_detect_init_before, body_detect_init_after, NULL);
