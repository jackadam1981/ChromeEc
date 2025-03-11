/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

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
#include <emul/emul_egis630.h>
#include <fingerprint/v4l2_types.h>
#include <fingerprint_egis630_private.h>

DEFINE_FFF_GLOBALS;

struct egis630_fixture {
	const struct device *dev;
	const struct emul *target;
};

static void *egis630_setup(void)
{
	static struct egis630_fixture fixture = {
		.dev = DEVICE_DT_GET(DT_NODELABEL(egis630)),
		.target = EMUL_DT_GET(DT_NODELABEL(egis630)),
	};

	zassert_not_null(fixture.dev);
	zassert_not_null(fixture.target);
	return &fixture;
}

ZTEST_SUITE(egis630, NULL, egis630_setup, NULL, NULL, NULL);

ZTEST_F(egis630, test_init_success)
{
	zassert_ok(fingerprint_init(fixture->dev));
	/*
	 * TODO(b/117620462): verify that sleep mode is WAI (no increased
	 * latency, expected power consumption).
	 */
	zassert_equal(egis630_get_low_power_mode(fixture->target), 0);
}

ZTEST_F(egis630, test_deinit_success)
{
	zassert_ok(fingerprint_deinit(fixture->dev));
}

ZTEST_F(egis630, test_get_info)
{
	struct fingerprint_info info;

	/* We need to initialize driver first to initialize 'error' field */
	zassert_ok(fingerprint_init(fixture->dev));
	zassert_ok(fingerprint_get_info(fixture->dev, &info));

	zassert_equal(info.vendor_id, FOURCC('E', 'G', 'I', 'S'));
	zassert_equal(info.product_id, 9);
	/*
	 * Last 4 bits of hardware id is a year of sensor production,
	 * could differ between sensors.
	 */
	// zassert_equal(info.model_id >> 4, 0x140);
	zassert_equal(info.version, 1);
	zassert_equal(info.frame_size, CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE);
	zassert_equal(info.pixel_format, FINGERPRINT_SENSOR_V4L2_PIXEL_FORMAT(
						 DT_NODELABEL(egis630)));
	zassert_equal(info.width,
		      FINGERPRINT_SENSOR_RES_X(DT_NODELABEL(egis630)));
	zassert_equal(info.height,
		      FINGERPRINT_SENSOR_RES_Y(DT_NODELABEL(egis630)));
	zassert_equal(info.bpp,
		      FINGERPRINT_SENSOR_RES_BPP(DT_NODELABEL(egis630)));
	zassert_equal(info.errors, FINGERPRINT_ERROR_DEAD_PIXELS_UNKNOWN |
					   FINGERPRINT_ERROR_NO_IRQ);
}

ZTEST_F(egis630, test_enter_idle)
{
	zassert_ok(fingerprint_set_mode(fixture->dev,
					FINGERPRINT_SENSOR_MODE_IDLE));
}

ZTEST_F(egis630, test_invalid_mode_not_supported)
{
	zassert_equal(fingerprint_set_mode(fixture->dev, UINT16_MAX), -ENOTSUP);
}

ZTEST_F(egis630, test_maintenance_not_supported)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE];

	zassert_equal(fingerprint_maintenance(fixture->dev, buffer,
					      sizeof(buffer)),
		      -ENOTSUP);
}

ZTEST_F(egis630, test_maintenance_image_small_buffer_size)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE - 1];

	zassert_equal(fingerprint_maintenance(fixture->dev, buffer,
					      sizeof(buffer)),
		      -EINVAL);
}

ZTEST_F(egis630, test_finger_status_not_supported)
{
	zassert_equal(fingerprint_finger_status(fixture->dev), -ENOTSUP);
}

ZTEST_F(egis630, test_acquire_image_not_supported)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE];

	zassert_equal(fingerprint_acquire_image(fixture->dev, 0, buffer,
						sizeof(buffer)),
		      -ENOTSUP);
}

ZTEST_F(egis630, test_acquire_image_small_buffer_size)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE] = { 0 };
	size_t image_buf_size = CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE - 1;
	int mode = FINGERPRINT_CAPTURE_TYPE_VENDOR_FORMAT;

	zassert_equal(fingerprint_acquire_image(fixture->dev, mode, buffer,
						image_buf_size),
		      -EINVAL);
}
