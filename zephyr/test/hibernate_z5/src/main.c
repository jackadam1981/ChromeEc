/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "system.h"
#include "zephyr/device.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(hibernate_z5, NULL, NULL, NULL, NULL, NULL);

#ifndef CONFIG_HIBERNATE_Z5_INIT_SHOULD_FAIL
ZTEST(hibernate_z5, test_hibernate_z5__assert_normal)
{
	const struct device *hibernate_dev =
		DEVICE_DT_GET(DT_NODELABEL(hibernate_z5));
	const struct gpio_dt_spec gpio_en_slp_z =
		GPIO_DT_SPEC_GET(DT_NODELABEL(hibernate_z5), en_slp_z_gpios);

	zassert_true(device_is_ready(hibernate_dev));

	gpio_pin_set_dt(&gpio_en_slp_z, 0);
	board_hibernate_late();
	zassert_true(
		gpio_emul_output_get(gpio_en_slp_z.port, gpio_en_slp_z.pin));
}
#else
ZTEST(hibernate_z5, test_hibernate_z5__assert_init_fail)
{
	const struct device *hibernate_dev =
		DEVICE_DT_GET(DT_NODELABEL(hibernate_z5));

	zassert_false(device_is_ready(hibernate_dev));
}
#endif
