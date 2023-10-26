/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "hooks.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(int, tablet_get_mode);

int tablet_get_mode_low_mock(void)
{
	return 0;
}

int tablet_get_mode_high_mock(void)
{
	return 1;
}

ZTEST(rex_ish_board, test_nb_mode_low)
{
	const struct gpio_dt_spec *gpio =
		GPIO_DT_FROM_NODELABEL(gpio_ec_nb_mode_l);

	tablet_get_mode_fake.custom_fake = tablet_get_mode_low_mock;
	hook_notify(HOOK_TABLET_MODE_CHANGE);
	zassert_equal(gpio_emul_output_get(gpio->port, gpio->pin), 0);
}

ZTEST(rex_ish_board, test_nb_mode_high)
{
	const struct gpio_dt_spec *gpio =
		GPIO_DT_FROM_NODELABEL(gpio_ec_nb_mode_l);

	tablet_get_mode_fake.custom_fake = tablet_get_mode_high_mock;
	hook_notify(HOOK_TABLET_MODE_CHANGE);
	zassert_equal(gpio_emul_output_get(gpio->port, gpio->pin), 1);
}

ZTEST_SUITE(rex_ish_board, NULL, NULL, NULL, NULL, NULL);
