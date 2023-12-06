/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "mock_fingerprint_algorithm.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>

#include <drivers/fingerprint.h>
#include <drivers/fingerprint_sim.h>
#include <ec_commands.h>
#include <ec_tasks.h>
#include <fpsensor/fpsensor_state.h>
#include <host_command.h>

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, mkbp_send_event, uint8_t);

#define fp_sim DEVICE_DT_GET(DT_CHOSEN(cros_fp_fingerprint_sensor))

ZTEST_USER(fpsensor, test_tpm_seed_init)
{
	struct ec_response_fp_encryption_status status;
	struct ec_params_fp_seed params = {
		.struct_version = 4,
		.reserved = 0,
		.seed = "very_secret_32_bytes_of_tpm_seed",
	};

	/* Get FP encryption flags. */
	zassert_ok(ec_cmd_fp_encryption_status(NULL, &status));

	/* Confirm TPM seed is not set */
	zassert_true(status.valid_flags & FP_ENC_STATUS_SEED_SET);
	zassert_false(status.status & FP_ENC_STATUS_SEED_SET);

	/* Set TPM seed. */
	zassert_ok(ec_cmd_fp_seed(NULL, &params));

	/* Get FP encryption flags. */
	zassert_ok(ec_cmd_fp_encryption_status(NULL, &status));

	/* Confirm that FP_ENC_STATUS_SEED_SET is set. */
	zassert_true(status.valid_flags & FP_ENC_STATUS_SEED_SET);
	zassert_true(status.status & FP_ENC_STATUS_SEED_SET);

	/* Try to set TPM seed once again (should fail). */
	zassert_equal(EC_RES_ACCESS_DENIED, ec_cmd_fp_seed(NULL, &params));
}

ZTEST_USER(fpsensor, test_tpm_seed_invalid)
{
	struct ec_params_fp_seed params = {
		/* 0 is not an actual structure version. */
		.struct_version = 0,
		.reserved = 0,
		.seed = "very_secret_32_bytes_of_tpm_seed",
	};

	/* Try to et TPM seed (should fail). */
	zassert_equal(EC_RES_INVALID_PARAM, ec_cmd_fp_seed(NULL, &params));
}

ZTEST_USER(fpsensor, test_set_fp_context)
{
	struct ec_params_fp_context_v1 params = {
		.action = FP_CONTEXT_ASYNC,
		.userid = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8},
	};
	struct ec_response_fp_encryption_status status;

	/* Set context (asynchronously). */
	zassert_ok(ec_cmd_fp_context_v1(NULL, &params));

	/* Now any attempt to set context should return EC_RES_BUSY. */
	zassert_equal(EC_RES_BUSY, ec_cmd_fp_context_v1(NULL, &params));

	/* Now any attempt to get command result should return EC_RES_BUSY. */
	params.action = FP_CONTEXT_GET_RESULT;
	zassert_equal(EC_RES_BUSY, ec_cmd_fp_context_v1(NULL, &params));

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Get command result. */
	zassert_ok(ec_cmd_fp_context_v1(NULL, &params));

	/* Get FP encryption flags. */
	zassert_ok(ec_cmd_fp_encryption_status(NULL, &status));

	/* Confirm that FP_CONTEXT_USER_ID_SET is set. */
	zassert_true(status.status & FP_CONTEXT_USER_ID_SET);
}

ZTEST_USER(fpsensor, test_maintenance_mode)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_SENSOR_MAINTENANCE,
	};
	struct ec_response_fp_mode response;
	struct ec_response_fp_info info;
	struct fingerprint_sensor_state state;

	const int dead_pixels = 3;

	/* Confirm that number of dead pixels is unknown. */
	zassert_ok(ec_cmd_fp_info(NULL, &info));
	zassert_equal(FP_ERROR_DEAD_PIXELS(info.errors), FP_ERROR_DEAD_PIXELS_UNKNOWN);

	fingerprint_get_state(fp_sim, &state);
	state.bad_pixels = dead_pixels;
	fingerprint_set_state(fp_sim, &state);

	/* Change fingerprint mode to maintenance. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_SENSOR_MAINTENANCE);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Check that maintenance was ran. */
	fingerprint_get_state(fp_sim, &state);
	zassert_true(state.maintenance_ran);

	/* Confirm that number of dead pixels is correct. */
	zassert_ok(ec_cmd_fp_info(NULL, &info));
	zassert_equal(FP_ERROR_DEAD_PIXELS(info.errors), dead_pixels);

	/*
	 * Confirm that maintenance flag is not set after the maintenance
	 * operation is finished.
	 */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_SENSOR_MAINTENANCE);
}


ZTEST_USER(fpsensor, test_finger_down_mode)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_FINGER_DOWN,
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;

	/* Detect finger on the sensor. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_DOWN);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Confirm that fpsensor task is waiting for finger. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_DOWN);

	/* Confirm that detect mode was enabled. */
	fingerprint_get_state(fp_sim, &state);
	zassert_true(state.detect_mode);

	/* Disable finger detection */
	params.mode = 0;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_FINGER_DOWN);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Confirm that detect mode was disabled. */
	fingerprint_get_state(fp_sim, &state);
	zassert_false(state.detect_mode);
}

ZTEST_USER(fpsensor, test_finger_down_present)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_FINGER_DOWN,
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;
	uint32_t fp_events;

	/* Detect finger on the sensor. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_DOWN);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Put finger on the sensor and ping fpsensor task. */
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	fingerprint_set_state(fp_sim, &state);
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task process event. */
	k_msleep(1);

	/* Confirm MKBP event was sent. */
	zassert_equal(mkbp_send_event_fake.call_count, 1);
	zassert_equal(mkbp_send_event_fake.arg0_val, EC_MKBP_EVENT_FINGERPRINT);
	fp_get_next_event((uint8_t *)&fp_events);
	zassert_true(fp_events & EC_MKBP_FP_FINGER_DOWN);

	/*
	 * Confirm that finger down flag is not set after the finger is
	 * detected.
	 */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_FINGER_DOWN);
}

ZTEST_USER(fpsensor, test_finger_down_partial)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_FINGER_DOWN,
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;

	/* Detect finger on the sensor. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_DOWN);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Put finger partially on the sensor and ping fpsensor task. */
	state.finger_state = FINGERPRINT_FINGER_STATE_PARTIAL;
	fingerprint_set_state(fp_sim, &state);
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task process event. */
	k_msleep(1);

	/* Confirm MKBP event was not sent. */
	zassert_equal(mkbp_send_event_fake.call_count, 0);

	/* Confirm that finger down flag is still set. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_DOWN);
}

ZTEST_USER(fpsensor, test_finger_down_no_finger)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_FINGER_DOWN,
	};
	struct ec_response_fp_mode response;

	/* Detect finger on the sensor. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_DOWN);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Ping fpsensor task, but don't put finger on the sensor. */
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task process event. */
	k_msleep(1);

	/* Confirm MKBP event was not sent. */
	zassert_equal(mkbp_send_event_fake.call_count, 0);

	/* Confirm that finger down flag is still set. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_DOWN);
}

ZTEST_USER(fpsensor, test_finger_up_mode)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_FINGER_UP,
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	fingerprint_set_state(fp_sim, &state);

	/* Detect finger up. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_UP);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Confirm that fpsensor task is waiting for finger up. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_UP);

	/* Confirm that detect mode was enabled. */
	fingerprint_get_state(fp_sim, &state);
	zassert_true(state.detect_mode);

	/* Disable finger up detection */
	params.mode = 0;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_FINGER_UP);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Confirm that detect mode was disabled. */
	fingerprint_get_state(fp_sim, &state);
	zassert_false(state.detect_mode);
}

ZTEST_USER(fpsensor, test_finger_up_no_finger_no_interrupt)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_FINGER_UP,
	};
	struct ec_response_fp_mode response;
	uint32_t fp_events;

	/* Detect finger up. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_UP);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Confirm that fpsensor task is waiting for finger up. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_UP);

	/* Check that no MKBP event was triggered yet. */
	zassert_equal(mkbp_send_event_fake.call_count, 0);

	/* Give at least 100ms for fpsensor to check the finger state. */
	k_msleep(101);

	/* Confirm MKBP event was sent. */
	zassert_equal(mkbp_send_event_fake.call_count, 1);
	zassert_equal(mkbp_send_event_fake.arg0_val, EC_MKBP_EVENT_FINGERPRINT);
	fp_get_next_event((uint8_t *)&fp_events);
	zassert_true(fp_events & EC_MKBP_FP_FINGER_UP);

	/*
	 * Confirm that finger down flag is not set after the finger is
	 * detected.
	 */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_FINGER_UP);
}

ZTEST_USER(fpsensor, test_finger_up_present_then_no_finger_interrupt)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_FINGER_UP,
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;
	uint32_t fp_events;

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	fingerprint_set_state(fp_sim, &state);

	/* Detect finger up. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_UP);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Leave finger off the sensor and ping fpsensor task. */
	state.finger_state = FINGERPRINT_FINGER_STATE_NONE;
	fingerprint_set_state(fp_sim, &state);
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task process event. */
	k_msleep(1);

	/* Confirm MKBP event was sent. */
	zassert_equal(mkbp_send_event_fake.call_count, 1);
	zassert_equal(mkbp_send_event_fake.arg0_val, EC_MKBP_EVENT_FINGERPRINT);
	fp_get_next_event((uint8_t *)&fp_events);
	zassert_true(fp_events & EC_MKBP_FP_FINGER_UP);

	/*
	 * Confirm that finger down flag is not set after the finger is
	 * detected.
	 */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_FINGER_UP);
}

ZTEST_USER(fpsensor, test_finger_up_present_then_no_finger_no_interrupt)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_FINGER_UP,
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;
	uint32_t fp_events;

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	fingerprint_set_state(fp_sim, &state);

	/* Detect finger up. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_UP);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Leave finger off the sensor and ping fpsensor task. */
	state.finger_state = FINGERPRINT_FINGER_STATE_NONE;
	fingerprint_set_state(fp_sim, &state);

	/* Give at least 100ms for fpsensor to check the finger state. */
	k_msleep(101);

	/* Confirm MKBP event was sent. */
	zassert_equal(mkbp_send_event_fake.call_count, 1);
	zassert_equal(mkbp_send_event_fake.arg0_val, EC_MKBP_EVENT_FINGERPRINT);
	fp_get_next_event((uint8_t *)&fp_events);
	zassert_true(fp_events & EC_MKBP_FP_FINGER_UP);

	/*
	 * Confirm that finger down flag is not set after the finger is
	 * detected.
	 */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_FINGER_UP);
}

ZTEST_USER(fpsensor, test_finger_up_present_then_partial_no_interrupt)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_FINGER_UP,
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	fingerprint_set_state(fp_sim, &state);

	/* Detect finger up. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_UP);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Put finger partially on the sensor. */
	state.finger_state = FINGERPRINT_FINGER_STATE_PARTIAL;
	fingerprint_set_state(fp_sim, &state);

	/* Give at least 100ms for fpsensor to check the finger state. */
	k_msleep(101);

	/* Confirm MKBP event was not sent. */
	zassert_equal(mkbp_send_event_fake.call_count, 0);

	/* Confirm that finger up flag is still set. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_UP);
}

ZTEST_USER(fpsensor, test_finger_up_partial_then_no_finger_no_interrupt)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_FINGER_UP,
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;
	uint32_t fp_events;

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PARTIAL;
	fingerprint_set_state(fp_sim, &state);

	/* Detect finger up. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_FINGER_UP);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Leave finger off the sensor and ping fpsensor task. */
	state.finger_state = FINGERPRINT_FINGER_STATE_NONE;
	fingerprint_set_state(fp_sim, &state);

	/* Give at least 100ms for fpsensor to check the finger state. */
	k_msleep(101);

	/* Confirm MKBP event was sent. */
	zassert_equal(mkbp_send_event_fake.call_count, 1);
	zassert_equal(mkbp_send_event_fake.arg0_val, EC_MKBP_EVENT_FINGERPRINT);
	fp_get_next_event((uint8_t *)&fp_events);
	zassert_true(fp_events & EC_MKBP_FP_FINGER_UP);

	/*
	 * Confirm that finger down flag is not set after the finger is
	 * detected.
	 */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_FINGER_UP);
}

ZTEST_USER(fpsensor, test_finger_capture)
{

}

static void *fpsensor_setup(void)
{
	/* Start shimmed tasks. */
	start_ec_tasks();
	k_msleep(100);

	return NULL;
}

static void fpsensor_before(void *f)
{
	struct fingerprint_sensor_state state = {
		.bad_pixels = 0,
		.maintenance_ran = false,
		.detect_mode = false,
		.low_power_mode = false,
		.finger_state = FINGERPRINT_FINGER_STATE_NONE,
	};

	fingerprint_set_state(fp_sim, &state);
	RESET_FAKE(mkbp_send_event);
}

ZTEST_SUITE(fpsensor, NULL, fpsensor_setup, fpsensor_before, NULL, NULL);
