/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery_fuel_gauge.h"
#include "battery_smart.h"
#include "common.h"
#include "util.h"

static enum battery_present batt_pres_prev = BP_NOT_SURE;

/*
 * Battery info for all Volteer battery types. Note that the fields
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
	/* SMP 996QA193H Battery Information */
	[BATTERY_SIMPLO_HIGHPOWER] = {
		.fuel_gauge = {
			.manuf_name = "333-1D-11-A",
			.ship_mode = {
				.reg_addr = 0x0,
				.reg_data = { 0x0010, 0x0010 },
			},
			.fet = {
				.mfgacc_support = 1,
				.reg_addr = 0x0,
				.reg_mask = 0x0006,
				.disconnect_val = 0x0,
			},
		},
		.batt_info = {
			.voltage_max = 13200,		/* mV */
			.voltage_normal = 11550,	/* mV */
			.voltage_min = 9000,		/* mV */
			.precharge_current = 256,	/* mA */
			.start_charging_min_c = 0,
			.start_charging_max_c = 45,
			.charging_min_c = 0,
			.charging_max_c = 45,
			.discharging_min_c = -20,
			.discharging_max_c = 60,
		},
	},
	/* Cosmx CA407792G Battery Information */
	[BATTERY_COSMX] = {
		.fuel_gauge = {
			.manuf_name = "333-AC-11-A",
			.ship_mode = {
				.reg_addr = 0x0,
				.reg_data = { 0x0010, 0x0010 },
			},
			.fet = {
				.mfgacc_support = 1,
				.reg_addr = 0x0,
				.reg_mask = 0x0006,
				.disconnect_val = 0x0,
			},
		},
		.batt_info = {
			.voltage_max = 13200,		/* mV */
			.voltage_normal = 11550,	/* mV */
			.voltage_min = 9000,		/* mV */
			.precharge_current = 256,	/* mA */
			.start_charging_min_c = 0,
			.start_charging_max_c = 45,
			.charging_min_c = 0,
			.charging_max_c = 45,
			.discharging_min_c = -10,
			.discharging_max_c = 60,
		},
	},
};
BUILD_ASSERT(ARRAY_SIZE(board_battery_info) == BATTERY_TYPE_COUNT);

const enum battery_type DEFAULT_BATTERY_TYPE = BATTERY_SIMPLO_HIGHPOWER;

static bool battery_init(void)
{
	int batt_status;

	return battery_status(&batt_status) ? 0 :
		!!(batt_status & STATUS_INITIALIZED);
}

static int battery_check_disconnect(void)
{
	if (!battery_init())
		return BATTERY_DISCONNECT_ERROR;

	return BATTERY_NOT_DISCONNECTED;
}

/*
 * Physical detection of battery.
 */
__override enum battery_present battery_check_present_status(void)
{
	enum battery_present batt_pres;
	int batt_disconnect_status;

	/* Get the physical hardware status */
	batt_pres = battery_hw_present();

	/*
	 * If the battery is not physically connected, then no need to perform
	 * any more checks.
	 */
	if (batt_pres != BP_YES)
		return batt_pres;

	/*
	 * If the battery is present now and was present last time we checked,
	 * return early.
	 */
	if (batt_pres == batt_pres_prev)
		return batt_pres;

	/*
	 * Check battery disconnect status. If we are unable to read battery
	 * disconnect status, then return BP_NOT_SURE. Battery could be in ship
	 * mode and might require pre-charge current to wake it up. BP_NO is not
	 * returned here because charger state machine will not provide
	 * pre-charge current assuming that battery is not present.
	 */
	batt_disconnect_status = battery_check_disconnect();
	if (batt_disconnect_status == BATTERY_DISCONNECT_ERROR)
		return BP_NOT_SURE;

	/*
	 * Ensure that battery is:
	 * 1. Not in cutoff
	 * 2. Initialized
	 */
	if (battery_is_cut_off() || !battery_init())
		batt_pres = BP_NO;

	return batt_pres;
}
