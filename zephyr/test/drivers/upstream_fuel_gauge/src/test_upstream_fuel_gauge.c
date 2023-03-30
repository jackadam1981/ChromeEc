/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"
#include "common.h"
#include "console.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_smart_battery.h"
#include "i2c.h"
#include "test/drivers/test_state.h"

#include <stdio.h>

#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/ztest.h>

#define BATTERY_NODE DT_NODELABEL(upstream_battery)

ZTEST(upstream_fuel_gauge, test_api_is_available)
{
	const struct device *dev = DEVICE_DT_GET_ANY(sbs_sbs_gauge_new_api);

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
