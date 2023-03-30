/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test/drivers/test_state.h"
#include "zephyr/device.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>

#define BATTERY_NODE DT_NODELABEL(upstream_battery)

ZTEST(upstream_fuel_gauge, test_api_is_available)
{
	const struct device *dev = DEVICE_DT_GET(BATTERY_NODE);

	struct fuel_gauge_get_property props[] = {
		{
			.property_type = FUEL_GAUGE_VOLTAGE,
		},
	};

	int ret = fuel_gauge_get_prop(dev, props, ARRAY_SIZE(props));

	for (int i = 0; i < ARRAY_SIZE(props); i++) {
		zassert_ok(props[i].status,
			   "Property %d getting %d has a bad status.", i,
			   props[i].property_type);
	}

	zassert_ok(ret);
}

ZTEST_SUITE(upstream_fuel_gauge, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);
