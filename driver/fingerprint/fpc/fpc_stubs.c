/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "fpsensor.h"
#include "fpsensor_types.h"

/*
 * Private is not defined, so create stubs for required functions from private
 * libraries
 */

void fp_sensor_configure_detect(void)
{
}

enum finger_state fp_sensor_finger_status(void)
{
	return FINGER_NONE;
}

int fp_sensor_acquire_image_with_mode(uint8_t *image_data, int mode)
{
	return EC_ERROR_UNIMPLEMENTED;
}

void fp_sensor_low_power(void)
{
}

int fpc_get_hwid(uint16_t *id)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int fpc_check_hwid(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

/* Reset and initialize the sensor IC */
int fp_sensor_init(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

/* Deinitialize the sensor IC */
int fp_sensor_deinit(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int fp_sensor_get_info(struct ec_response_fp_info *resp)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int fp_finger_match(void *templ, uint32_t templ_count, uint8_t *image,
		    int32_t *match_index, uint32_t *update_bitmap)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int fp_enrollment_begin(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int fp_enrollment_finish(void *templ)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int fp_finger_enroll(uint8_t *image, int *completion)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int fp_maintenance(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

struct fp_sensor_interface fp_driver_stub = {
	.sensor_type = FP_SENSOR_TYPE_FPC,
	.sensor_hwid = FP_SENSOR_HWID_FPC,
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
	.image_size = FP_SENSOR_IMAGE_SIZE_FPC,
	.template_size = FP_ALGORITHM_TEMPLATE_SIZE_FPC,
	.encrypted_template_size =
		FP_ALGORITHM_TEMPLATE_SIZE_FPC + FP_POSITIVE_MATCH_SALT_BYTES +
		sizeof(struct ec_fp_template_encryption_metadata),
	.res_x = FP_SENSOR_RES_X_FPC,
	.res_y = FP_SENSOR_RES_Y_FPC
};

struct fp_sensor_interface *fpc_sensor_get_interface(void)
{
	return &fp_driver_stub;
}
