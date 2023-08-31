/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpc1025_private.h"

#include <drivers/cros_fingerprint.h>

const char sensor_library_version[] = "**mocked**";
const char sensor_library_build_info[] = "**none**";

/* FPC specific initialization and de-initialization functions */
int fp_sensor_open(void)
{
	return 0;
}

int fp_sensor_close(void)
{
	return 0;
}

const char *fp_sensor_get_version(void)
{
	return sensor_library_version;
}

const char *fp_sensor_get_build_info(void)
{
	return sensor_library_build_info;
}

int fp_sensor_maintenance(uint8_t *image_data, fp_sensor_info_t *fp_sensor_info)
{
	return -ENOTSUP;
}

int fp_sensor_acquire_image_with_mode(uint8_t *image_data, int mode)
{
	return -ENOTSUP;
}

void fp_sensor_configure_detect(void)
{
	return;
}

int fp_sensor_finger_status(void)
{
	return FINGER_STATE_NONE;
}
