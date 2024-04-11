/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "dps.h"
#include "dt-bindings/battery.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "zephyr/kernel.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(struct dps_config_t *, dps_get_config);
FAKE_VALUE_FUNC(int, battery_status, int *);
__overridable struct dps_config_t dps_config;

struct dps_config_t *dps_get_config_mock(void)
{
	return &dps_config;
}

int battery_status_value;
int battery_status_mock(int *status)
{
	return battery_status_value;
}

ZTEST(is_more_efficient, test_squirtle_is_more_efficient)
{
	dps_get_config_fake.custom_fake = dps_get_config_mock;
	battery_status_fake.custom_fake = battery_status_mock;

	struct dps_config_t *config = dps_get_config();

	/*
	 * Guaranteed to be at optimum power.
	 */
	battery_status_value = 0;
	int curr_mv = 15000;
	int prev_mv = 12000;
	int batt_mv = 12000;

	zassert_equal(
		config->is_more_efficient(curr_mv, prev_mv, batt_mv, 0, 0),
		false, "squirtle_is_more_efficient=%d",
		config->is_more_efficient(curr_mv, prev_mv, batt_mv, 0, 0));
}
ZTEST_SUITE(is_more_efficient, NULL, NULL, NULL, NULL, NULL);
