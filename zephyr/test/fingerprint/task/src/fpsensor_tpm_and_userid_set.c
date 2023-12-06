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

ZTEST_USER(fpsensor_tpm_and_userid_set, test_enroll_start_stop)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_ENROLL_SESSION | FP_MODE_ENROLL_IMAGE,
	};
	struct ec_response_fp_mode response;

	/* Start enroll session. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & (FP_MODE_ENROLL_SESSION | FP_MODE_ENROLL_IMAGE));

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Make sure the 'enroll_start' callback was called. */
	zassert_equal(mock_alg_enroll_start_fake.call_count, 1);

	/* Stop enroll session. */
	params.mode = 0;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & (FP_MODE_ENROLL_SESSION | FP_MODE_ENROLL_IMAGE));

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Confirm that capture mode is not enabled. */
	params.mode = FP_MODE_DONT_CHANGE;
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_false(response.mode & FP_MODE_ENROLL_SESSION);

	/* Make sure the 'enroll_finish' callback was called. */
	zassert_equal(mock_alg_enroll_start_fake.call_count, 1);
}

#if 0
ZTEST_USER(fpsensor_tpm_and_userid_set, test_enroll)
{
	struct ec_params_fp_mode params = {
		.mode = FP_MODE_ENROLL_SESSION | FP_MODE_ENROLL_IMAGE,
	};
	struct ec_response_fp_mode response;
	struct fingerprint_sensor_state state;
	struct ec_params_fp_frame frame_request = {
		.offset = FP_FRAME_INDEX_RAW_IMAGE << FP_FRAME_INDEX_SHIFT,
		.size = image_size,
	};

	/* Switch mode to enroll. */
	zassert_ok(ec_cmd_fp_mode(NULL, &params, &response));
	zassert_true(response.mode & (FP_MODE_ENROLL_SESSION | FP_MODE_ENROLL_IMAGE));

	/* Give opportunity for fpsensor task to change mode. */
	k_msleep(1);

	/* Make sure the algorithm is in enroll session */
	zassert_equal(mock_alg_enroll_start_fake.call_count, 1);

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
#endif

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

	RESET_FAKE(mock_alg_init);
	RESET_FAKE(mock_alg_exit);
	RESET_FAKE(mock_alg_enroll_start);
	RESET_FAKE(mock_alg_enroll_step);
	RESET_FAKE(mock_alg_enroll_finish);
	RESET_FAKE(mock_alg_match);
}

ZTEST_SUITE(fpsensor_tpm_and_userid_set, NULL, fpsensor_setup, fpsensor_before, NULL, NULL);
