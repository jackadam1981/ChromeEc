/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "battery_smart.h"
#include "gpio.h"

/* Shutdown mode parameter to write to manufacturer access register */
#define SB_SHIP_MODE_REG	0x3a
#define SB_SHUTDOWN_DATA	0xC574

/* Battery info for proto */
static const struct battery_info info = {
	/* TODO(waihong): Copied from Lux. Need verify. */
	.voltage_max = 8800,
	.voltage_normal = 7700,
	.voltage_min = 6100,
	.precharge_current = 256, /* mA */
	.start_charging_min_c = 0,
	.start_charging_max_c = 45,
	.charging_min_c = 0,
	.charging_max_c = 45,
	.discharging_min_c = -10,
	.discharging_max_c = 60,
};

const struct battery_info *battery_get_info(void)
{
	return &info;
}

int board_cut_off_battery(void)
{
	/* TODO(waihong): Implement battary cut-off. */
	return EC_SUCCESS;
}
