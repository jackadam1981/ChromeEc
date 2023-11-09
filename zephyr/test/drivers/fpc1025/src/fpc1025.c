/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/fingerprint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>

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

ZTEST_SUITE(fpc1025, NULL, fpc1025_setup, NULL, NULL, NULL);

ZTEST_F(fpc1025, test_init_success)
{
	zassert_ok(fingerprint_init(fixture->dev));
}
