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
FAKE_VALUE_FUNC(int, system_is_locked);

#define fp_sim DEVICE_DT_GET(DT_CHOSEN(cros_fp_fingerprint_sensor))
#define image_size                          \
	FINGERPRINT_SENSOR_REAL_IMAGE_SIZE( \
		DT_CHOSEN(cros_fp_fingerprint_sensor))
uint8_t image_buffer[image_size];
uint8_t frame_buffer[image_size];

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
		.userid = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8 },
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
	zassert_equal(FP_ERROR_DEAD_PIXELS(info.errors),
		      FP_ERROR_DEAD_PIXELS_UNKNOWN);

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

ZTEST_USER(fpsensor, test_finger_capture_simple_image_detection_enabled)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_CAPTURE |
			(FP_CAPTURE_SIMPLE_IMAGE << FP_MODE_CAPTURE_TYPE_SHIFT),
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;

	/* Switch mode to capture. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_CAPTURE);
	zassert_equal(FP_CAPTURE_TYPE(response.mode), FP_CAPTURE_SIMPLE_IMAGE);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Confirm that detect mode was enabled. */
	fingerprint_get_state(fp_sim, &state);
	zassert_true(state.detect_mode);

	/* Disable finger capture. */
	params.mode = 0;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_CAPTURE);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Confirm that detect mode was disabled. */
	fingerprint_get_state(fp_sim, &state);
	zassert_false(state.detect_mode);
}

ZTEST_USER(fpsensor, test_finger_capture_simple_image_mode_is_correct)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_CAPTURE |
			(FP_CAPTURE_SIMPLE_IMAGE << FP_MODE_CAPTURE_TYPE_SHIFT),
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;

	/* Switch mode to capture. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_CAPTURE);
	zassert_equal(FP_CAPTURE_TYPE(response.mode), FP_CAPTURE_SIMPLE_IMAGE);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	fingerprint_set_state(fp_sim, &state);

	/* Ping fpsensor task. */
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task process event. */
	k_msleep(1);

	/* Confirm that correct mode was passed to driver. */
	fingerprint_get_state(fp_sim, &state);
	zassert_equal(state.last_acquire_image_mode,
		      FINGERPRINT_CAPTURE_TYPE_SIMPLE_IMAGE);
}

ZTEST_USER(fpsensor, test_finger_capture_finger_state_partial)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_CAPTURE |
			(FP_CAPTURE_SIMPLE_IMAGE << FP_MODE_CAPTURE_TYPE_SHIFT),
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;

	/* Switch mode to capture. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_CAPTURE);
	zassert_equal(FP_CAPTURE_TYPE(response.mode), FP_CAPTURE_SIMPLE_IMAGE);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Put finger on the sensor (partially). */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PARTIAL;
	fingerprint_set_state(fp_sim, &state);

	/* Ping fpsensor task. */
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task process event. */
	k_msleep(1);

	/* Confirm that no scan was performed. */
	fingerprint_get_state(fp_sim, &state);
	zassert_equal(state.last_acquire_image_mode, -1);

	/* Confirm that capture mode is still enabled. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_CAPTURE);
}

ZTEST_USER(fpsensor, test_finger_capture_finger_state_none)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_CAPTURE |
			(FP_CAPTURE_SIMPLE_IMAGE << FP_MODE_CAPTURE_TYPE_SHIFT),
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;

	/* Switch mode to capture. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_CAPTURE);
	zassert_equal(FP_CAPTURE_TYPE(response.mode), FP_CAPTURE_SIMPLE_IMAGE);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Ping fpsensor task. */
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task process event. */
	k_msleep(1);

	/* Confirm that no scan was performed. */
	fingerprint_get_state(fp_sim, &state);
	zassert_equal(state.last_acquire_image_mode, -1);

	/* Confirm that capture mode is still enabled. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_CAPTURE);
}

ZTEST_USER(fpsensor, test_finger_capture_simple_image_scan_too_fast)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_CAPTURE |
			(FP_CAPTURE_SIMPLE_IMAGE << FP_MODE_CAPTURE_TYPE_SHIFT),
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;

	/* Switch mode to capture. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_CAPTURE);
	zassert_equal(FP_CAPTURE_TYPE(response.mode), FP_CAPTURE_SIMPLE_IMAGE);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	state.acquire_image_result = FINGERPRINT_SENSOR_SCAN_TOO_FAST;
	fingerprint_set_state(fp_sim, &state);

	/* Ping fpsensor task. */
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task process event. */
	k_msleep(1);

	/* Confirm that capture mode is still enabled. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_CAPTURE);
}

ZTEST_USER(fpsensor, test_finger_capture_simple_image_scan_success_mode_cleared)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_CAPTURE |
			(FP_CAPTURE_SIMPLE_IMAGE << FP_MODE_CAPTURE_TYPE_SHIFT),
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;

	/* Switch mode to capture. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_CAPTURE);
	zassert_equal(FP_CAPTURE_TYPE(response.mode), FP_CAPTURE_SIMPLE_IMAGE);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	fingerprint_set_state(fp_sim, &state);

	/* Ping fpsensor task. */
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task process event. */
	k_msleep(1);

	/* Confirm that capture mode is not enabled. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_CAPTURE);
}

ZTEST_USER(fpsensor, test_finger_capture_simple_image_scan_success_mkbp_event)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_CAPTURE |
			(FP_CAPTURE_SIMPLE_IMAGE << FP_MODE_CAPTURE_TYPE_SHIFT),
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;
	uint32_t fp_events;

	/* Switch mode to match. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_CAPTURE);
	zassert_equal(FP_CAPTURE_TYPE(response.mode), FP_CAPTURE_SIMPLE_IMAGE);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	fingerprint_set_state(fp_sim, &state);

	/* Ping fpsensor task. */
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task to process event. */
	k_msleep(1);

	/* Confirm MKBP event was sent. */
	zassert_equal(mkbp_send_event_fake.call_count, 1);
	zassert_equal(mkbp_send_event_fake.arg0_val, EC_MKBP_EVENT_FINGERPRINT);

	/* Confirm that FP_IMAGE_READY MKBP event is sent. */
	fp_get_next_event((uint8_t *)&fp_events);
	zassert_true(fp_events & EC_MKBP_FP_IMAGE_READY);
}

ZTEST_USER(fpsensor, test_finger_capture_simple_image_scan_success_get_frame)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_CAPTURE |
			(FP_CAPTURE_SIMPLE_IMAGE << FP_MODE_CAPTURE_TYPE_SHIFT),
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;
	struct ec_params_fp_frame frame_request = {
		.offset = FP_FRAME_INDEX_RAW_IMAGE << FP_FRAME_INDEX_SHIFT,
		.size = image_size,
	};

	/* Switch mode to capture. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_CAPTURE);
	zassert_equal(FP_CAPTURE_TYPE(response.mode), FP_CAPTURE_SIMPLE_IMAGE);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	fingerprint_set_state(fp_sim, &state);

	/* Prepare image. */
	for (int i = 0; i < image_size; i++)
		image_buffer[i] = 1;

	/* Load image to simulator. */
	fingerprint_load_image(fp_sim, image_buffer, image_size);

	/* Ping fpsensor task. */
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task process event. */
	k_msleep(1);

	/* Get fingerprint raw image and compare buffers. */
	zassert_ok(ec_cmd_fp_frame(NULL, &frame_request, frame_buffer));
	zassert_mem_equal(frame_buffer, image_buffer, image_size);
}

ZTEST_USER(fpsensor, test_fp_frame_raw_image_system_is_locked)
{
	struct ec_params_fp_frame frame_request = {
		.offset = FP_FRAME_INDEX_RAW_IMAGE << FP_FRAME_INDEX_SHIFT,
		.size = image_size,
	};

	/* Lock the system. */
	system_is_locked_fake.return_val = true;

	/*
	 * Confirm that it's not possible to get raw image when system is
	 * locked.
	 */
	zassert_equal(ec_cmd_fp_frame(NULL, &frame_request, frame_buffer),
		      EC_RES_ACCESS_DENIED);
}

ZTEST_USER(fpsensor, test_fp_frame_raw_image_size_too_big)
{
	struct ec_params_fp_frame frame_request = {
		.offset = FP_FRAME_INDEX_RAW_IMAGE << FP_FRAME_INDEX_SHIFT,
		.size = image_size + 1,
	};

	/*
	 * Confirm that FP_FRAME host command will return an error when
	 * requested more than fingerprint frame size.
	 */
	zassert_equal(ec_cmd_fp_frame(NULL, &frame_request, frame_buffer), EC_RES_INVALID_PARAM);
}

ZTEST_USER(fpsensor, test_fp_frame_raw_image_bad_offset)
{
	struct ec_params_fp_frame frame_request = {
		.offset = ((FP_FRAME_INDEX_RAW_IMAGE << FP_FRAME_INDEX_SHIFT) | (image_size + 1)),
		.size = 1,
	};

	/*
	 * Confirm that FP_FRAME host command will return an error when
	 * trying to read from bad offset.
	 */
	zassert_equal(ec_cmd_fp_frame(NULL, &frame_request, frame_buffer), EC_RES_INVALID_PARAM);
}

ZTEST_USER(fpsensor, test_finger_match_no_templates_mkbp_event)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_MATCH,
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;
	uint32_t fp_events;

	/* Switch mode to match. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_MATCH);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	fingerprint_set_state(fp_sim, &state);

	/* Ping fpsensor task. */
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task to process event. */
	k_msleep(1);

	/* Confirm MKBP event was sent. */
	zassert_equal(mkbp_send_event_fake.call_count, 1);
	zassert_equal(mkbp_send_event_fake.arg0_val, EC_MKBP_EVENT_FINGERPRINT);

	/*
	 * Confirm that:
	 * - MKBP event is FP_MATCH
	 * - Match failed with NO_TEMPLATES
	 * - Finger ID is FP_NO_SUCH_TEMPLATE
	 */
	fp_get_next_event((uint8_t *)&fp_events);
	zassert_true(fp_events & EC_MKBP_FP_MATCH);
	zassert_equal(EC_MKBP_FP_ERRCODE(fp_events), EC_MKBP_FP_ERR_MATCH_NO_TEMPLATES);
	zassert_equal(EC_MKBP_FP_MATCH_IDX(fp_events), FP_NO_SUCH_TEMPLATE & 0xF);
}

ZTEST_USER(fpsensor, test_finger_match_no_templates_mode_cleared)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_MATCH,
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;

	/* Switch mode to match. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & FP_MODE_MATCH);

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Put finger on the sensor. */
	fingerprint_get_state(fp_sim, &state);
	state.finger_state = FINGERPRINT_FINGER_STATE_PRESENT;
	fingerprint_set_state(fp_sim, &state);

	/* Ping fpsensor task. */
	fingerprint_run_callback(fp_sim);

	/* Give opportunity for fpsensor task to process event. */
	k_msleep(1);

	/* Confirm that capture mode is not enabled. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_MATCH);
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
		.acquire_image_result = FINGERPRINT_SENSOR_SCAN_GOOD,
		.last_acquire_image_mode = -1,
	};

	fingerprint_set_state(fp_sim, &state);
	RESET_FAKE(mkbp_send_event);
	RESET_FAKE(system_is_locked);
}

ZTEST_SUITE(fpsensor, NULL, fpsensor_setup, fpsensor_before, NULL, NULL);
