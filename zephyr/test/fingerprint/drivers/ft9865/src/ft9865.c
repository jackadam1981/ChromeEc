/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ft9865_pal_test_helpers.h"

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
#include <emul/emul_ft9865.h>
#include <fingerprint/v4l2_types.h>
#include <fingerprint_ft9865.h>
#include <fingerprint_ft9865_private.h>

DEFINE_FFF_GLOBALS;

struct ft9865_fixture {
	const struct device *dev;
	const struct emul *target;
};

static void *ft9865_setup(void)
{
	static struct ft9865_fixture fixture = {
		.dev = DEVICE_DT_GET(DT_NODELABEL(ft9865)),
		.target = EMUL_DT_GET(DT_NODELABEL(ft9865)),
	};

	zassert_not_null(fixture.dev);
	zassert_not_null(fixture.target);
	return &fixture;
}

ZTEST_SUITE(ft9865, NULL, ft9865_setup, NULL, NULL, NULL);

ZTEST_F(ft9865, test_init_success)
{
	zassert_ok(fingerprint_init(fixture->dev));
}

ZTEST_F(ft9865, test_deinit_success)
{
	zassert_ok(fingerprint_deinit(fixture->dev));
}

ZTEST_F(ft9865, test_get_info)
{
	struct fingerprint_info info;

	zassert_ok(fingerprint_init(fixture->dev));
	zassert_ok(fingerprint_get_info(fixture->dev, &info));

	zassert_equal(info.vendor_id, FOURCC('F', 'T', ' ', ' '));
	zassert_equal(info.product_id, 9);
	zassert_equal(info.version, 1);
	zassert_equal(info.frame_size, CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE);
	zassert_equal(info.pixel_format, FINGERPRINT_SENSOR_V4L2_PIXEL_FORMAT(
						 DT_NODELABEL(ft9865)));
	zassert_equal(info.width,
		      FINGERPRINT_SENSOR_RES_X(DT_NODELABEL(ft9865)));
	zassert_equal(info.height,
		      FINGERPRINT_SENSOR_RES_Y(DT_NODELABEL(ft9865)));
	zassert_equal(info.bpp,
		      FINGERPRINT_SENSOR_RES_BPP(DT_NODELABEL(ft9865)));
	zassert_equal(info.errors, FINGERPRINT_ERROR_DEAD_PIXELS_UNKNOWN);
}

ZTEST_F(ft9865, test_enter_idle)
{
	zassert_ok(fingerprint_set_mode(fixture->dev,
					FINGERPRINT_SENSOR_MODE_IDLE));
}

ZTEST_F(ft9865, test_invalid_mode_not_supported)
{
	zassert_equal(fingerprint_set_mode(fixture->dev, UINT16_MAX), -ENOTSUP);
}

ZTEST_F(ft9865, test_maintenance_not_supported)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE];

	zassert_equal(fingerprint_maintenance(fixture->dev, buffer,
					      sizeof(buffer)),
		      -ENOTSUP);
}

ZTEST_F(ft9865, test_maintenance_image_small_buffer_size)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE - 1];

	zassert_equal(fingerprint_maintenance(fixture->dev, buffer,
					      sizeof(buffer)),
		      -EINVAL);
}

ZTEST_F(ft9865, test_finger_status_not_supported)
{
	zassert_equal(fingerprint_finger_status(fixture->dev), -ENOTSUP);
}

ZTEST_F(ft9865, test_acquire_image_not_supported)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE];

	zassert_equal(fingerprint_acquire_image(fixture->dev, 0, buffer,
						sizeof(buffer)),
		      -ENOTSUP);
}

ZTEST_F(ft9865, test_acquire_image_small_buffer_size)
{
	uint8_t buffer[CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE] = { 0 };
	size_t image_buf_size = CONFIG_FINGERPRINT_SENSOR_IMAGE_SIZE - 1;
	enum fingerprint_capture_type capture_type =
		FINGERPRINT_CAPTURE_TYPE_VENDOR_FORMAT;

	zassert_equal(fingerprint_acquire_image(fixture->dev, capture_type,
						buffer, image_buf_size),
		      -EINVAL);
}

ZTEST_F(ft9865, test_periphery_spi_write_read_success)
{
	uint8_t tx_buf[1] = { 0xFD };
	uint8_t rx_buf[3] = { 0 };
	uint8_t expeceted_rx_buf[3] = { 0x1, 0x65, 0x98 };

	zassert_ok(fingerprint_init(fixture->dev));
	zassert_ok(ft9865_spi_write_then_read(tx_buf, sizeof(tx_buf),
						    rx_buf, sizeof(rx_buf)));
	zassert_mem_equal(rx_buf, expeceted_rx_buf, sizeof(expeceted_rx_buf),
			  "Received data does not match sent data");
}

ZTEST_F(ft9865, test_periphery_spi_write_read_spi_stopped)
{
	uint8_t tx_buf[1] = { 0xFD };
	uint8_t rx_buf[3] = { 0 };

	zassert_ok(fingerprint_init(fixture->dev));
	ft9865_stop_spi(fixture->target);
	zassert_equal(ft9865_spi_write_then_read(tx_buf, sizeof(tx_buf),
						       rx_buf, sizeof(rx_buf)),
		      -EINVAL);
}

ZTEST_F(ft9865, test_periphery_spi_write_success)
{
	uint8_t tx_buf[1] = { 0xFE };

	zassert_ok(fingerprint_init(fixture->dev));
	zassert_ok(ft9865_spi_write(tx_buf, sizeof(tx_buf)));
}

ZTEST_F(ft9865, test_periphery_spi_write_stopped)
{
	uint8_t tx_buf[1] = { 0xFD };

	zassert_ok(fingerprint_init(fixture->dev));
	ft9865_stop_spi(fixture->target);
	zassert_equal(ft9865_spi_write(tx_buf, sizeof(tx_buf)), -EINVAL);
}

ZTEST_F(ft9865, test_sensor_hw_reset)
{
	zassert_ok(fingerprint_init(fixture->dev));
	zassert_ok(ft9865_sensor_hw_reset());
}