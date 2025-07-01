/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "system.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(hibernate_z5, NULL, NULL, NULL, NULL, NULL);

ZTEST(hibernate_z5, test_hibernate_z5_assert)
{
	const struct gpio_dt_spec gpio_en_slp_z =
		GPIO_DT_SPEC_GET(DT_NODELABEL(hibernate_z5), en_slp_z_gpios);

	gpio_pin_set_dt(&gpio_en_slp_z, 0);
	board_hibernate_late();
	zassert_true(
		gpio_emul_output_get(gpio_en_slp_z.port, gpio_en_slp_z.pin));
}
