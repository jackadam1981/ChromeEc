/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "system.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(enum ec_error_list, isl9238c_hibernate, int);

ZTEST_SUITE(hibernate, NULL, NULL, NULL, NULL, NULL);

ZTEST(hibernate, test_board_hibernate)
{
	RESET_FAKE(isl9238c_hibernate);

	board_hibernate();
	zassert_equal(isl9238c_hibernate_fake.call_count, 1);
	zassert_equal(isl9238c_hibernate_fake.arg0_val, 0);
}
