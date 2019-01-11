/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery_fuel_gauge.h"
#include "battery_smart.h"
#include "charge_manager.h"
#include "chipset.h"
#include "common.h"
#include "hooks.h"
#include "usb_pd.h"

/*
 * Battery info for all ampton/apel battery types. Note that the fields
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
	[BATTERY_C214] = {
		.fuel_gauge = {
			.manuf_name = "AS1GUXd3KB",
			.device_name = "C214-43",
			.ship_mode = {
				.reg_addr = 0x0,
				.reg_data = { 0x10, 0x10 },
			},
			.fet = {
				.mfgacc_support = 1,
				.reg_addr = 0x00,
				.reg_mask = 0x2000,
				.disconnect_val = 0x2000,
			},
		},
		.batt_info = {
			.voltage_max = 13200,
			.voltage_normal = 11550,
			.voltage_min = 9000,
			.precharge_current = 256,
			.start_charging_min_c = 0,
			.start_charging_max_c = 45,
			.charging_min_c = 0,
			.discharging_min_c = 0,
			.discharging_max_c = 60,
		},
	},
	[BATTERY_C204EE] = {
		.fuel_gauge = {
			.manuf_name = "AS1GVCD3KB",
			.device_name = "C204-35",
			.ship_mode = {
				.reg_addr = 0x0,
				.reg_data = { 0x10, 0x10 },
			},
			.fet = {
				.mfgacc_support = 1,
				.reg_addr = 0x00,
				.reg_mask = 0x2000,
				.disconnect_val = 0x2000,
			},
		},
		.batt_info = {
			.voltage_max = 13200,
			.voltage_normal = 11550,
			.voltage_min = 9000,
			.precharge_current = 256,
			.start_charging_min_c = 0,
			.start_charging_max_c = 45,
			.charging_min_c = 0,
			.discharging_min_c = 0,
			.discharging_max_c = 60,
		},
	},
};
BUILD_ASSERT(ARRAY_SIZE(board_battery_info) == BATTERY_TYPE_COUNT);

const enum battery_type DEFAULT_BATTERY_TYPE = BATTERY_C214;

static void reduce_input_voltage_when_full(void)
{
	struct batt_params batt;
	int max_pd_voltage_mv;
	int active_chg_port;

	active_chg_port = charge_manager_get_active_charge_port();
	if (active_chg_port == CHARGE_PORT_NONE)
		return;

	battery_get_params(&batt);
	if (!(batt.flags & BATT_FLAG_BAD_STATUS)) {
		/* Lower our input voltage to 5V when battery is full. */
		if ((batt.status & STATUS_FULLY_CHARGED) &&
		    chipset_in_state(CHIPSET_STATE_ANY_OFF))
			max_pd_voltage_mv = 5000;
		else
			max_pd_voltage_mv = PD_MAX_VOLTAGE_MV;

		if (pd_get_max_voltage() != max_pd_voltage_mv)
			pd_set_external_voltage_limit(active_chg_port,
						      max_pd_voltage_mv);
	}
}
DECLARE_HOOK(HOOK_SECOND, reduce_input_voltage_when_full, HOOK_PRIO_DEFAULT);
