/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Mock fpsensor private driver
 */

#include "common.h"
#include "fpsensor.h"
#include "fpsensor_driver.h"
#include "mock/fpsensor_mock.h"

#include <stdlib.h>

#ifndef TEST_BUILD
#error "Mocks should only be in the test build."
#endif

struct mock_ctrl_fp_sensor mock_ctrl_fp_sensor = MOCK_CTRL_DEFAULT_FP_SENSOR;

int fp_sensor_init(void)
{
	return mock_ctrl_fp_sensor.fp_sensor_init_return;
}

int fp_sensor_deinit(void)
{
	return mock_ctrl_fp_sensor.fp_sensor_deinit_return;
}

int fp_sensor_get_info(struct ec_response_fp_info *resp)
{
	resp->version = 0;
	return mock_ctrl_fp_sensor.fp_sensor_get_info_return;
}

void fp_sensor_low_power(void)
{
}

void fp_sensor_configure_detect(void)
{
}

enum finger_state fp_sensor_finger_status(void)
{
	return mock_ctrl_fp_sensor.fp_sensor_finger_status_return;
}

int fp_sensor_acquire_image(uint8_t *image_data)
{
	return mock_ctrl_fp_sensor.fp_sensor_acquire_image_return;
}

int fp_sensor_acquire_image_with_mode(uint8_t *image_data, int mode)
{
	return mock_ctrl_fp_sensor.fp_sensor_acquire_image_with_mode_return;
}

int fp_finger_match(void *templ, uint32_t templ_count, uint8_t *image,
		    int32_t *match_index, uint32_t *update_bitmap)
{
	return mock_ctrl_fp_sensor.fp_finger_match_return;
}

int fp_enrollment_begin(void)
{
	return mock_ctrl_fp_sensor.fp_enrollment_begin_return;
}

int fp_enrollment_finish(void *templ)
{
	return mock_ctrl_fp_sensor.fp_enrollment_finish_return;
}

int fp_finger_enroll(uint8_t *image, int *completion)
{
	return mock_ctrl_fp_sensor.fp_finger_enroll_return;
}

int fp_maintenance(void)
{
	return mock_ctrl_fp_sensor.fp_maintenance_return;
}

struct fp_sensor_interface fp_driver_mock = {
	.sensor_type = FP_SENSOR_TYPE_UNKNOWN,
	.sensor_init = &fp_sensor_init,
	.sensor_deinit = &fp_sensor_deinit,
	.sensor_get_info = &fp_sensor_get_info,
	.sensor_low_power = &fp_sensor_low_power,
	.sensor_configure_detect = &fp_sensor_configure_detect,
	.sensor_finger_status = &fp_sensor_finger_status,
	.sensor_acquire_image_with_mode = &fp_sensor_acquire_image_with_mode,
	.finger_enroll = &fp_finger_enroll,
	.finger_match = &fp_finger_match,
	.enrollment_begin = &fp_enrollment_begin,
	.enrollment_finish = &fp_enrollment_finish,
	.maintenance = &fp_maintenance,
	.image_size = FP_SENSOR_IMAGE_SIZE,
	.template_size = 0,
	.encrypted_template_size =
		FP_POSITIVE_MATCH_SALT_BYTES +
		sizeof(struct ec_fp_template_encryption_metadata),
	.res_x = 0,
	.res_y = 0
};

/* Fp sensor driver interface. */
struct fp_sensor_interface *fp_driver = &fp_driver_mock;

struct fp_sensor_interface *fpc_sensor_get_interface(void)
{
	return &fp_driver_mock;
}
