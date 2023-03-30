/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "test/drivers/test_state.h"
#include "zephyr/device.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>

ZTEST(upstream_fuel_gauge, test_battery_get_params__success)
{
	struct batt_params ret_params = {0};

	battery_get_params(&ret_params);

	zassert_equal(ret_params.voltage, 1);
}

ZTEST_SUITE(upstream_fuel_gauge, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);
