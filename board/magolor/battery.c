/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */
#include "battery.h"
#include "battery_fuel_gauge.h"
#include "charge_state.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "util.h"

#define CHECK_BATT_STAT_DELAY_MS (150 * MSEC)

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ##args)
/*
 * Battery info for magolor battery types. Note that the fields
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
const struct batt_conf_embed board_battery_info[] = {
		/* LGC AP18C8K Battery Information */
	[BATTERY_LGC_AP18C8K] = {
		.manuf_name = "LGC KT0030G020",
		.device_name = "AP18C8K",
		.config = {
			.fuel_gauge = {
				.ship_mode = {
					.reg_addr = 0x3A,
					.reg_data = { 0xC574, 0xC574 },
				},
				.fet = {
					.reg_addr = 0x43,
					.reg_mask = 0x0001,
					.disconnect_val = 0x0,
					.cfet_mask = 0x0002,
					.cfet_off_val = 0x0000,
				},
			},
			.batt_info = {
				.voltage_max            = 13050,
				.voltage_normal         = 11250,
				.voltage_min            = 9000,
				.precharge_current      = 256,
				.start_charging_min_c   = 0,
				.start_charging_max_c   = 50,
				.charging_min_c         = 0,
				.charging_max_c         = 60,
				.discharging_min_c      = -20,
				.discharging_max_c      = 75,
			},
		},
	},
	/* Murata AP18C4K Battery Information */
	[BATTERY_MURATA_AP18C4K] = {
		.manuf_name = "Murata KT00304012",
		.device_name = "AP18C4K",
		.config = {
			.fuel_gauge = {
				.ship_mode = {
					.reg_addr = 0x3A,
					.reg_data = { 0xC574, 0xC574 },
				},
				.fet = {
					.reg_addr = 0x0,
					.reg_mask = 0x2000,
					.disconnect_val = 0x2000,
					.cfet_mask = 0x4000,
					.cfet_off_val = 0x4000,
				},
			},
			.batt_info = {
				.voltage_max		= 13200,
				.voltage_normal		= 11400,
				.voltage_min		= 9000,
				.precharge_current	= 256,
				.start_charging_min_c	= 0,
				.start_charging_max_c	= 50,
				.charging_min_c		= 0,
				.charging_max_c		= 60,
				.discharging_min_c	= -20,
				.discharging_max_c	= 75,
			},
		},
	},
	/* AP19B8M */
	[BATTERY_AP19B8M] = {
		.manuf_name = "LGC KT0030G024",
		.config = {
			.fuel_gauge = {
				.ship_mode = {
					.reg_addr = 0x3A,
					.reg_data = { 0xC574, 0xC574 },
				},
				.fet = {
					.reg_addr = 0x43,
					.reg_mask = 0x0001,
					.disconnect_val = 0x0,
					.cfet_mask = 0x0002,
					.cfet_off_val = 0x0000,
				},
			},
			.batt_info = {
				.voltage_max          = 13350,
				.voltage_normal       = 11610,
				.voltage_min          = 9000,
				.precharge_current    = 256,
				.start_charging_min_c = 0,
				.start_charging_max_c = 50,
				.charging_min_c       = 0,
				.charging_max_c       = 60,
				.discharging_min_c    = -20,
				.discharging_max_c    = 75,
			},
		},
	},
	/* AP18C7M */
	[BATTERY_AP18C7M] = {
		.manuf_name = "SMP KT00407008",
		.config = {
			.fuel_gauge = {
				.ship_mode = {
					.reg_addr = 0x3A,
					.reg_data = { 0xC574, 0xC574 },
				},
				.fet = {
					.reg_addr = 0x0,
					.reg_mask = 0x0002,
					.disconnect_val = 0x0000,
					.cfet_mask = 0x4000,
					.cfet_off_val = 0x4000,
				},
				.flags = FUEL_GAUGE_FLAG_MFGACC,
			},
			.batt_info = {
				.voltage_max          = 17600,
				.voltage_normal       = 15400,
				.voltage_min          = 12000,
				.precharge_current    = 256,
				.start_charging_min_c = 0,
				.start_charging_max_c = 45,
				.charging_min_c       = 0,
				.charging_max_c       = 60,
				.discharging_min_c    = -20,
				.discharging_max_c    = 70,
			},
		},
	},
	/* COSMX AP20CBL Battery Information */
	[BATTERY_COSMX_AP20CBL] = {
		.manuf_name = "COSMX KT0030B002",
		.device_name = "AP20CBL",
		.config = {
			.fuel_gauge = {
				.ship_mode = {
					.reg_addr = 0x3A,
					.reg_data = { 0xC574, 0xC574 },
				},
				.fet = {
					.reg_addr = 0x0,
					.reg_mask = 0x2000,
					.disconnect_val = 0x2000,
					.cfet_mask = 0x4000,
					.cfet_off_val = 0x4000,
				},
				.flags = FUEL_GAUGE_FLAG_MFGACC,
			},
			.batt_info = {
				.voltage_max            = 13200,
				.voltage_normal         = 11550,
				.voltage_min            = 9000,
				.precharge_current      = 256,
				.start_charging_min_c   = 0,
				.start_charging_max_c   = 50,
				.charging_min_c         = 0,
				.charging_max_c         = 60,
				.discharging_min_c      = -20,
				.discharging_max_c      = 75,
			},
		},
	},
	/* COSMX AP20CBL Battery Information (new firmware ver) */
	[BATTERY_COSMX_AP20CBL_004] = {
		.manuf_name = "COSMX KT0030B004",
		.device_name = "AP20CBL",
		.config = {
			.fuel_gauge = {
				.ship_mode = {
					.reg_addr = 0x3A,
					.reg_data = { 0xC574, 0xC574 },
				},
				.fet = {
					.reg_addr = 0x0,
					.reg_mask = 0x2000,
					.disconnect_val = 0x2000,
					.cfet_mask = 0x4000,
					.cfet_off_val = 0x4000,
				},
				.flags = FUEL_GAUGE_FLAG_MFGACC,
			},
			.batt_info = {
				.voltage_max            = 13200,
				.voltage_normal         = 11550,
				.voltage_min            = 9000,
				.precharge_current      = 256,
				.start_charging_min_c   = 0,
				.start_charging_max_c   = 50,
				.charging_min_c         = 0,
				.charging_max_c         = 60,
				.discharging_min_c      = -20,
				.discharging_max_c      = 75,
			},
		},
	},
	/* LGES AP23A8L Battery Information */
	[BATTERY_AP23A8L] = {
		.manuf_name = "LGES KT0030G025",
		.device_name = "AP23A8L",
		.config = {
			.fuel_gauge = {
				.ship_mode = {
					.reg_addr = 0x3A,
					.reg_data = { 0xC574, 0xC574 },
				},
				.fet = {
					.reg_addr = 0x0,
					.reg_mask = 0x8000,
					.disconnect_val = 0x0000,
					.cfet_mask = 0x4000,
					.cfet_off_val = 0x0000,
				},
			},
			.batt_info = {
				.voltage_max		= 13200,
				.voltage_normal		= 11280, /* mV */
				.voltage_min		= 9000, /* mV */
				.precharge_current	= 256,	/* mA */
				.start_charging_min_c	= 0,
				.start_charging_max_c	= 50,
				.charging_min_c		= 0,
				.charging_max_c		= 60,
				.discharging_min_c	= -20,
				.discharging_max_c	= 75,
			},
		},
	},
};
BUILD_ASSERT(ARRAY_SIZE(board_battery_info) == BATTERY_TYPE_COUNT);

const enum battery_type DEFAULT_BATTERY_TYPE = BATTERY_LGC_AP18C8K;

#ifdef BOARD_MAGOLOR
static int check_batt_retry;
void board_check_battery_status(void)
{
	enum battery_disconnect_state battery_disconnect_status =
		battery_get_disconnect_state();

	/*
	 * The following 2 states can read DFET status successfully
	 * so not need to do initialize battery type again.
	 * BATTERY_DISCONNECTED: The DFET is off.
	 * BATTERY_NOT_DISCONNECTED: The battery can discharge.
	 * Therefore, do initilialize only at BATTERY_DISCONNECT_ERROR.
	 */
	if (battery_disconnect_status != BATTERY_DISCONNECT_ERROR) {
		check_batt_retry = 0;
		return;
	}

	check_batt_retry++;
	if (check_batt_retry > 5) {
		CPRINTS("Board has retried init_battery_type 5 times.");
		check_batt_retry = 0;
		return;
	}

	CPRINTS("Retry init_battery_type: %d", check_batt_retry);
	init_battery_type();
}
DECLARE_DEFERRED(board_check_battery_status);

__override int board_get_default_battery_type(void)
{
	const struct batt_params *batt = charger_current_battery_params();

	if (batt->flags & BATT_FLAG_RESPONSIVE) {
		/* Check Battery status again after 500msec. */
		hook_call_deferred(&board_check_battery_status_data,
				   CHECK_BATT_STAT_DELAY_MS);
	} else {
		check_batt_retry = 0;
	}

	return DEFAULT_BATTERY_TYPE;
}
#endif
