/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "cbi_battery_params.h"

struct board_batt_params cbi_board_battery_info[] = {
	/* POW-TECH GQA05 Battery Information */
	[CBI_BATTERY_POWER_TECH] = {
		/* BQ40Z50 Fuel Gauge */
		.fuel_gauge = {
			.manuf_name = "POW-TECH",
			.device_name = "BATGQA05L22",
			.ship_mode = {
				.reg_addr = 0x00,
				.reg_data = { 0x0010, 0x0010 },
			},
			.fet = {
				.mfgacc_support = 1,
				.reg_addr = 0x00,
				.reg_mask = 0x2000,		/* XDSG */
				.disconnect_val = 0x2000,
			}
		},
		.batt_info = {
			.voltage_max		= TARGET_WITH_MARGIN(13050, 5),
			.voltage_normal		= 11400, /* mV */
			.voltage_min		= 9000, /* mV */
			.precharge_current	= 280,	/* mA */
			.start_charging_min_c	= 0,
			.start_charging_max_c	= 45,
			.charging_min_c		= 0,
			.charging_max_c		= 45,
			.discharging_min_c	= -10,
			.discharging_max_c	= 60,
		},
	},
};
BUILD_ASSERT(ARRAY_SIZE(cbi_board_battery_info) == CBI_BATTERY_TYPE_COUNT);
