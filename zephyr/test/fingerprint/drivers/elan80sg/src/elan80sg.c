// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "elan80sg_pal_test_helpers.h"

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

/* Converts capture types from the ec domain to the fpc domain. */
enum elan_capture_type convert_fp_capture_type_to_elan_capture_type(
	enum fingerprint_capture_type mode);

ZTEST_SUITE(elan80sg, NULL, elan80sg_setup, NULL, NULL, NULL);

ZTEST_F(elan80sg, test_deinit_success)
{
	zassert_ok(fingerprint_deinit(fixture->dev));
}

ZTEST_F(elan80sg, test_enter_idle)
{
	zassert_ok(fingerprint_set_mode(fixture->dev,
					FINGERPRINT_SENSOR_MODE_IDLE));
}

ZTEST_F(elan80sg, test_invalid_mode_not_supported)
{
	zassert_equal(fingerprint_set_mode(fixture->dev, UINT16_MAX), -ENOTSUP);
}

ZTEST_F(elan80sg, test_maintenance_image_small_buffer_size)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE - 1];

	zassert_equal(fingerprint_maintenance(fixture->dev, buffer,
					      sizeof(buffer)),
		      -EINVAL);
}

ZTEST_F(elan80sg, test_maintenance_not_supported)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE];

	zassert_equal(fingerprint_maintenance(fixture->dev, buffer,
					      sizeof(buffer)),
		      0);
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

ZTEST_F(elan80sg, test_elan_usleep)
{
	uint64_t usecs = 30000;
	uint64_t tick_usecs = USEC_PER_SEC / CONFIG_SYS_CLOCK_TICKS_PER_SEC;
	uint64_t t1, t2;

	/* k_uptime_get() returns system uptime in milliseconds. */
	t1 = k_uptime_get();
	zassert_ok(elan80sg_elan_usleep(usecs));
	t2 = k_uptime_get();

	zassert_within((t2 - t1) * USEC_PER_MSEC, usecs, tick_usecs);
}

ZTEST_F(elan80sg, test_elan_malloc)
{
	size_t size = sizeof(int);
	void *void_ptr = elan80sg_elan_malloc(size);
	zassert_not_null(void_ptr, "sys_alloc should return a valid pointer");

	int *int_ptr = (int *)void_ptr;
	int num = 4546;
	*int_ptr = num;
	zassert_equal(*int_ptr, num);

	elan80sg_elan_free(void_ptr);
}

ZTEST_F(elan80sg, test_convert_fp_capture_type_to_elan_capture_type)
{
	zassert_equal(convert_fp_capture_type_to_elan_capture_type(
			      FINGERPRINT_CAPTURE_TYPE_VENDOR_FORMAT),
		      ELAN_CAPTURE_VENDOR_FORMAT);
	zassert_equal(convert_fp_capture_type_to_elan_capture_type(
			      FINGERPRINT_CAPTURE_TYPE_SIMPLE_IMAGE),
		      ELAN_CAPTURE_SIMPLE_IMAGE);
	zassert_equal(convert_fp_capture_type_to_elan_capture_type(
			      FINGERPRINT_CAPTURE_TYPE_PATTERN0),
		      ELAN_CAPTURE_PATTERN0);
	zassert_equal(convert_fp_capture_type_to_elan_capture_type(
			      FINGERPRINT_CAPTURE_TYPE_PATTERN1),
		      ELAN_CAPTURE_PATTERN1);
	zassert_equal(convert_fp_capture_type_to_elan_capture_type(
			      FINGERPRINT_CAPTURE_TYPE_QUALITY_TEST),
		      ELAN_CAPTURE_QUALITY_TEST);
	zassert_equal(convert_fp_capture_type_to_elan_capture_type(
			      FINGERPRINT_CAPTURE_TYPE_RESET_TEST),
		      ELAN_CAPTURE_RESET_TEST);
	zassert_equal(convert_fp_capture_type_to_elan_capture_type(
			      FINGERPRINT_CAPTURE_TYPE_MAX),
		      ELAN_CAPTURE_TYPE_INVALID);
}
