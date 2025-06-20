// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #include "elan80sg_pal_test_helpers.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>

#include <drivers/fingerprint.h>
#include <emul/emul_elan80sg.h>
#include <fingerprint/v4l2_types.h>
#include <fingerprint_elan80sg.h>
#include <fingerprint_elan80sg_private.h>

DEFINE_FFF_GLOBALS;

struct elan80sg_fixture {
	const struct device *dev;
	const struct emul *target;
};

static void *elan80sg_setup(void)
{
	static struct elan80sg_fixture fixture = {
		.dev = DEVICE_DT_GET(DT_NODELABEL(elan80sg)),
		.target = EMUL_DT_GET(DT_NODELABEL(elan80sg)),
	};

	zassert_not_null(fixture.dev);
	zassert_not_null(fixture.target);
	return &fixture;
}

ZTEST_SUITE(elan80sg, NULL, elan80sg_setup, elan80sg_before, NULL, NULL);

ZTEST_F(elan80sg, test_enter_idle)
{
	zassert_ok(fingerprint_set_mode(fixture->dev,
					FINGERPRINT_SENSOR_MODE_IDLE));
}

ZTEST_F(elan80sg, test_invalid_mode_not_supported)
{
	zassert_equal(fingerprint_set_mode(fixture->dev, UINT16_MAX), -ENOTSUP);
}

ZTEST_F(elan80sg, test_maintenance_not_supported)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE];

	zassert_equal(fingerprint_maintenance(fixture->dev, buffer,
					      sizeof(buffer)),
		      -ENOTSUP);
}

ZTEST_F(elan80sg, test_maintenance_image_small_buffer_size)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE - 1];

	zassert_equal(fingerprint_maintenance(fixture->dev, buffer,
					      sizeof(buffer)),
		      -EINVAL);
}

ZTEST_F(elan80sg, test_finger_status_not_supported)
{
	zassert_equal(fingerprint_finger_status(fixture->dev), -ENOTSUP);
}

ZTEST_F(elan80sg, test_acquire_image_not_supported)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE];

	zassert_equal(fingerprint_acquire_image(fixture->dev, 0, buffer,
						sizeof(buffer)),
		      -ENOTSUP);
}

ZTEST_F(elan80sg, test_acquire_image_small_buffer_size)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE] = { 0 };
	size_t image_buf_size = CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE - 1;
	enum fingerprint_capture_type capture_type =
		FINGERPRINT_CAPTURE_TYPE_VENDOR_FORMAT;

	zassert_equal(fingerprint_acquire_image(fixture->dev, capture_type,
						buffer, image_buf_size),
		      -EINVAL);
}
