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
