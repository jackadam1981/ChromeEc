/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "charge_state.h"
#include "hooks.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

__override int board_get_default_battery_type(void)
{
	int prev_battery_type = board_get_prev_battery_type();

	if (prev_battery_type < 0)
		return DEFAULT_BATTERY_TYPE;

	LOG_INF("previous battery_type=%d", prev_battery_type);
	return prev_battery_type;
}
