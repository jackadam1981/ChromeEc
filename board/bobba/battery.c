/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery_fuel_gauge.h"
#include "common.h"
#include "util.h"
#include "battery_smart.h"
#include "hooks.h"
#include "console.h"

/*
 * Battery info for all bobba battery types. Note that the fields
 * start_charging_min/max and charging_min/max are not used for the charger.
 * The effective temperature limits are given by discharging_min/max_c.
 *
 * Fuel Gauge (FG) parameters which are used for determining if the battery
 * is connected, the appropriate ship mode (battery cutoff) command, and the
 * charge/discharge FETs status.
 *
 * Ship mode (battery cutoff) requires 2 writes to the appropriate smart battery
 * register. For some batteries, the charge/discharge FET bits are set when
 * charging/discharging is active, in other types, these bits set mean that
 * charging/discharging is disabled. Therefore, in addition to the mask for
 * these bits, a disconnect value must be specified. Note that for TI fuel
 * gauge, the charge/discharge FET status is found in Operation Status (0x54),
 * but a read of Manufacturer Access (0x00) will return the lower 16 bits of
 * Operation status which contains the FET status bits.
 *
 * The assumption for battery types supported is that the charge/discharge FET
 * status can be read with a sb_read() command and therefore, only the register
 * address, mask, and disconnect value need to be provided.
 */
const struct board_batt_params board_battery_info[] = {
	/* LGC AC15A8J Battery Information */
	[BATTERY_LGC15] = {
		.fuel_gauge = {
			.manuf_name = "LGC",
			.device_name = "AC15A8J",
			.ship_mode = {
				.reg_addr = 0x3A,
				.reg_data = { 0xC574, 0xC574 },
			},
			.fet = {
				.mfgacc_support = 1,
				.reg_addr = 0x0,
				.reg_mask = 0x0002,
				.disconnect_val = 0x0,
			}
		},
		.batt_info = {
			.voltage_max		= 13200,
			.voltage_normal		= 11520, /* mV */
			.voltage_min		= 9000, /* mV */
			.precharge_current	= 256,	/* mA */
			.start_charging_min_c	= 0,
			.start_charging_max_c	= 50,
			.charging_min_c		= 0,
			.charging_max_c		= 60,
			.discharging_min_c	= 0,
			.discharging_max_c	= 60,
		},
	},

	/* Panasonic AP1505L Battery Information */
	[BATTERY_PANASONIC] = {
		.fuel_gauge = {
			.manuf_name = "PANASONIC",
			.ship_mode = {
				.reg_addr = 0x3A,
				.reg_data = { 0xC574, 0xC574 },
			},
			.fet = {
				.reg_addr = 0x0,
				.reg_mask = 0x4000,
				.disconnect_val = 0x0,
			}
		},
		.batt_info = {
			.voltage_max		= 13200,
			.voltage_normal		= 11550, /* mV */
			.voltage_min		= 9000, /* mV */
			.precharge_current	= 256,	/* mA */
			.start_charging_min_c	= 0,
			.start_charging_max_c	= 50,
			.charging_min_c		= 0,
			.charging_max_c		= 60,
			.discharging_min_c	= 0,
			.discharging_max_c	= 60,
		},
	},

	/* SANYO AC15A3J Battery Information */
	[BATTERY_SANYO] = {
		.fuel_gauge = {
			.manuf_name = "SANYO",
			.ship_mode = {
				.reg_addr = 0x3A,
				.reg_data = { 0xC574, 0xC574 },
			},
			.fet = {
				.reg_addr = 0x0,
				.reg_mask = 0x4000,
				.disconnect_val = 0x0,
			}
		},
		.batt_info = {
			.voltage_max		= TARGET_WITH_MARGIN(13200, 5),
			.voltage_normal		= 11550, /* mV */
			.voltage_min		= 9000, /* mV */
			.precharge_current	= 256,	/* mA */
			.start_charging_min_c	= 0,
			.start_charging_max_c	= 50,
			.charging_min_c		= 0,
			.charging_max_c		= 60,
			.discharging_min_c	= 0,
			.discharging_max_c	= 60,
		},
	},

	/* Sony Ap13J4K Battery Information */
	[BATTERY_SONY] = {
		.fuel_gauge = {
			.manuf_name = "SONYCorp",
			.ship_mode = {
				.reg_addr = 0x3A,
				.reg_data = { 0xC574, 0xC574 },
			},
			.fet = {
				.reg_addr = 0x0,
				.reg_mask = 0x8000,
				.disconnect_val = 0x8000,
			}
		},
		.batt_info = {
			.voltage_max		= TARGET_WITH_MARGIN(13200, 5),
			.voltage_normal		= 11400, /* mV */
			.voltage_min		= 9000, /* mV */
			.precharge_current	= 256,	/* mA */
			.start_charging_min_c	= 0,
			.start_charging_max_c	= 50,
			.charging_min_c		= 0,
			.charging_max_c		= 60,
			.discharging_min_c	= 0,
			.discharging_max_c	= 60,
		},
	},

	/* Simplo AP13J7K Battery Information */
	[BATTERY_SMP_AP13J7K] = {
		.fuel_gauge = {
			.manuf_name = "SIMPLO",
			.device_name = "AP13J7K",
			.ship_mode = {
				.reg_addr = 0x3A,
				.reg_data = { 0xC574, 0xC574 },
			},
			.fet = {
				.mfgacc_support = 1,
				.reg_addr = 0x0,
				.reg_mask = 0x0002,
				.disconnect_val = 0x0000,
			}
		},
		.batt_info = {
			.voltage_max		= 13050,
			.voltage_normal		= 11400, /* mV */
			.voltage_min		= 9000, /* mV */
			.precharge_current	= 256,	/* mA */
			.start_charging_min_c	= 0,
			.start_charging_max_c	= 45,
			.charging_min_c		= 0,
			.charging_max_c		= 60,
			.discharging_min_c	= 0,
			.discharging_max_c	= 60,
		},
	},
};
BUILD_ASSERT(ARRAY_SIZE(board_battery_info) == BATTERY_TYPE_COUNT);

const enum battery_type DEFAULT_BATTERY_TYPE = BATTERY_PANASONIC;

static void battery_check_dfet(void)
{
	int rv;

	rv = battery_get_disconnect_state();

	if (rv == BATTERY_DISCONNECT_ERROR)
		ccprintf("\tBATTERY_DISCONNECT_ERROR\n");
	else if (rv == BATTERY_DISCONNECTED)
		ccprintf("\tBATTERY_DISCONNECTED\n");
	else if (rv == BATTERY_NOT_DISCONNECTED)
		ccprintf("\tBATTERY_NOT_DISCONNECTED\n");
	else
		ccprintf("\tbattery_check_dfet unknown error\n");
}
DECLARE_HOOK(HOOK_TICK, battery_check_dfet, HOOK_PRIO_DEFAULT);

static int battery_check_normal_voltage(int argc, char **argv)
{
	int rv;
	int reg;

	rv = sb_read(0x19, &reg);

	if (rv)
		ccprintf("\tCheck normal voltage error.\n");
	else
		ccprintf("\tNormal voltage is %u mV.\n", reg);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(batt_norm_vol, battery_check_normal_voltage,
				"NULL", "check normal voltage from register");