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

/* Converts capture type modes from the ec domain to the fpc domain. */
int convert_fp_capture_mode_to_fpc_get_image_type(int mode);

ZTEST_SUITE(egis630, NULL, egis630_setup, NULL, NULL, NULL);

ZTEST_F(egis630, test_deinit_success)
{
	zassert_ok(fingerprint_deinit(fixture->dev));
}
