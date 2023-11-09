/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>

#include <drivers/fingerprint.h>
#include <emul/emul_fpc1025.h>
#include <fingerprint/v4l2_types.h>

struct fpc1025_fixture {
	const struct device *dev;
	const struct emul *target;
};

static void *fpc1025_setup(void)
{
	static struct fpc1025_fixture fixture = {
		.dev = DEVICE_DT_GET(DT_NODELABEL(fpc1025)),
		.target = EMUL_DT_GET(DT_NODELABEL(fpc1025)),
	};

	zassert_not_null(fixture.dev);
	zassert_not_null(fixture.target);
	return &fixture;
}

static void fpc1025_after(void *f)
{
	struct fpc1025_fixture *fixture = (struct fpc1025_fixture *)f;

	fpc1025_emul_reset(fixture->target);
}

ZTEST_SUITE(fpc1025, NULL, fpc1025_setup, NULL, fpc1025_after, NULL);

ZTEST_F(fpc1025, test_init_success)
{
	zassert_ok(fingerprint_init(fixture->dev));
	zassert_equal(fpc1025_get_low_power_mode(fixture->target), 1);
}

ZTEST_F(fpc1025, test_init_failure_bad_hwid)
{
	fpc1025_set_hwid(fixture->target, 0x0);
	zassert_equal(fingerprint_init(fixture->dev), -EINVAL);
}

ZTEST_F(fpc1025, test_deinit_success)
{
	zassert_ok(fingerprint_deinit(fixture->dev));
}

ZTEST_F(fpc1025, test_get_info)
{
	struct fingerprint_info info;

	/* We need to initialize driver first to initialize 'error' field */
	zassert_ok(fingerprint_init(fixture->dev));
	zassert_ok(fingerprint_get_info(fixture->dev, &info));

	zassert_equal(info.vendor_id, FOURCC('F', 'P', 'C', ' '));
	zassert_equal(info.product_id, 9);
	zassert_equal(info.model_id, 0x021f);
	zassert_equal(info.version, 1);
	zassert_equal(info.frame_size, CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE);
	zassert_equal(info.pixel_format, FINGERPRINT_SENSOR_V4L2_PIXEL_FORMAT(
						 DT_NODELABEL(fpc1025)));
	zassert_equal(info.width,
		      FINGERPRINT_SENSOR_RES_X(DT_NODELABEL(fpc1025)));
	zassert_equal(info.height,
		      FINGERPRINT_SENSOR_RES_Y(DT_NODELABEL(fpc1025)));
	zassert_equal(info.bpp,
		      FINGERPRINT_SENSOR_RES_BPP(DT_NODELABEL(fpc1025)));
	zassert_equal(info.errors, FINGERPRINT_ERROR_DEAD_PIXELS_UNKNOWN);
}

ZTEST_F(fpc1025, test_enter_low_power_mode)
{
	zassert_ok(fingerprint_set_mode(fixture->dev,
					FINGERPRINT_SENSOR_MODE_LOW_POWER));
	zassert_equal(fpc1025_get_low_power_mode(fixture->target), 1);
}

ZTEST_F(fpc1025, test_enter_idle)
{
	zassert_ok(fingerprint_set_mode(fixture->dev,
					FINGERPRINT_SENSOR_MODE_IDLE));
}

ZTEST_F(fpc1025, test_invalid_mode_not_supported)
{
	zassert_equal(fingerprint_set_mode(fixture->dev, UINT16_MAX), -ENOTSUP);
}

FAKE_VOID_FUNC(test_interrupt_handler, const struct device *);

ZTEST_F(fpc1025, test_interrupt)
{
	const struct gpio_dt_spec spec =
		GPIO_DT_SPEC_GET(DT_NODELABEL(fpc1025), irq_gpios);

	RESET_FAKE(test_interrupt_handler);
	zassert_ok(fingerprint_config(fixture->dev, test_interrupt_handler));

	/* Enable interrupt (they are disabled by default). */
	zassert_ok(gpio_pin_interrupt_configure_dt(&spec,
						   GPIO_INT_EDGE_TO_ACTIVE));

	/*
	 * Toggle the GPIO twice. We expect that the driver will disable
	 * interrupt in interrupt handler, so handler should be called once.
	 */
	for (int i = 0; i < 2; i++) {
		gpio_emul_input_set(spec.port, spec.pin, 1);
		k_msleep(5);
		gpio_emul_input_set(spec.port, spec.pin, 0);
		k_msleep(5);
	}

	/* Verify the handler was called once. */
	zassert_equal(test_interrupt_handler_fake.call_count, 1);
}

ZTEST_F(fpc1025, test_maintenance_not_supported)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE];

	zassert_equal(fingerprint_maintenance(fixture->dev, buffer,
					      sizeof(buffer)),
		      -ENOTSUP);
}

ZTEST_F(fpc1025, test_finger_status_not_supported)
{
	zassert_equal(fingerprint_finger_status(fixture->dev), -ENOTSUP);
}

ZTEST_F(fpc1025, test_acquire_image_not_supported)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE];

	zassert_equal(fingerprint_acquire_image(fixture->dev, 0, buffer,
						sizeof(buffer)),
		      -ENOTSUP);
}

ZTEST_F(fpc1025, test_sensor_mode_detect_not_supported)
{
	zassert_equal(fingerprint_set_mode(fixture->dev,
					   FINGERPRINT_SENSOR_MODE_DETECT),
		      -ENOTSUP);
}
