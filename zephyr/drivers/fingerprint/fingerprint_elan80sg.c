/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT elan_elan80sg

#include "fingerprint_elan80sg.h"
#include "fingerprint_elan80sg_private.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

#include <drivers/fingerprint.h>
#include <fingerprint/v4l2_types.h>

LOG_MODULE_REGISTER(cros_fingerprint, LOG_LEVEL_INF);

static enum elan_capture_type
convert_fp_capture_mode_to_elan_get_image_type(enum fp_capture_type mode)
{
	switch (mode) {
	case FINGERPRINT_CAPTURE_TYPE_VENDOR_FORMAT:
		return ELAN_CAPTURE_VENDOR_FORMAT;
	case FINGERPRINT_CAPTURE_TYPE_SIMPLE_IMAGE:
		return ELAN_CAPTURE_SIMPLE_IMAGE;
	case FINGERPRINT_CAPTURE_TYPE_PATTERN0:
		return ELAN_CAPTURE_PATTERN0;
	case FINGERPRINT_CAPTURE_TYPE_PATTERN1:
		return ELAN_CAPTURE_PATTERN1;
	case FINGERPRINT_CAPTURE_TYPE_QUALITY_TEST:
		return ELAN_CAPTURE_QUALITY_TEST;
	case FINGERPRINT_CAPTURE_TYPE_RESET_TEST:
		return ELAN_CAPTURE_RESET_TEST;
	default:
		return ELAN_CAPTURE_TYPE_INVALID;
	}
}
