/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Unit tests for program/nissa/src/board_power.c.
 */
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(int, extpower_is_present);

static void before_test(void *fixture)
{
	RESET_FAKE(extpower_is_present);
}

ZTEST_SUITE(nissa_board_power, NULL, NULL, before_test, NULL, NULL);

ZTEST(nissa_board_power, test_nop)
{
}
