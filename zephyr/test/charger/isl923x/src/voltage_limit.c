/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "voltage_limit.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(isl923x_voltage_limit, NULL, NULL, NULL, NULL, NULL);

ZTEST(isl923x_voltage_limit, test_calculate_max_voltage_func)
{
	// Test cases for calculate_max_voltage function
	// Test case with val = 0, sku_id = 0
	zassert_equal(DEFAULT_POWER, calculate_max_voltage(0, 0),
		      "Incorrect max voltage for val = 0, sku_id = 0");

	// Test case with val = 0, sku_id = 0xFFFFFF
	zassert_equal(DEFAULT_POWER, calculate_max_voltage(0, 0xFFFFFF),
		      "Incorrect max voltage for val = 0, sku_id = 0xFFFFFF");

	// Test case with val = 1, sku_id = 0
	zassert_equal(MAX_POWER_65W, calculate_max_voltage(1, 0),
		      "Incorrect max voltage for val = 1, sku_id = 0");

	// Test case with val = 0, sku_id = 0x2A0000
	zassert_equal(MAX_POWER_65W, calculate_max_voltage(0, 0x2A0000),
		      "Incorrect max voltage for val = 0, sku_id = 0x2A0000");

	// Test case with val = 1, sku_id = 0x2A0000
	zassert_equal(MAX_POWER_65W, calculate_max_voltage(1, 0x2A0000),
		      "Incorrect max voltage for val = 1, sku_id = 0x2A0000");

	// Test case with val = 0, sku_id = 0x2A0010
	zassert_equal(MAX_POWER_65W, calculate_max_voltage(0, 0x2A0010),
		      "Incorrect max voltage for val = 0, sku_id = 0x2A0010");

	// Test case with val = 1, sku_id = 0x2A0010
	zassert_equal(MAX_POWER_65W, calculate_max_voltage(1, 0x2A0010),
		      "Incorrect max voltage for val = 1, sku_id = 0x2A0010");
}
