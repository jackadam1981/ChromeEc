/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery_fuel_gauge.h"
#include "charge_state.h"
#include "common.h"

/*
 * Battery info for all sasukette battery types. Note that the fields
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
	/* SDI Battery Information */
	[BATTERY_SDI] = {
		.fuel_gauge = {
			.manuf_name = "SDI",
			.device_name = "4402D51",
			.ship_mode = {
				.reg_addr = 0x00,
				.reg_data = { 0x0010, 0x0010 },
			},
			.fet = {
				.mfgacc_support = 0,
				.reg_addr = 0x00,
				.reg_mask = 0xc000,
				.disconnect_val = 0x8000,
				.cfet_mask = 0xc000,
				.cfet_off_val = 0x2000,
			}
		},
		.batt_info = {
			.voltage_max		= 8860,
			.voltage_normal		= 7700, /* mV */
			.voltage_min		= 6000, /* mV */
			.precharge_current	= 200,	/* mA */
			.start_charging_min_c	= 0,
			.start_charging_max_c	= 45,
			.charging_min_c		= 0,
			.charging_max_c		= 50,
			.discharging_min_c	= -20,
			.discharging_max_c	= 70,
		},
	}
};
BUILD_ASSERT(ARRAY_SIZE(board_battery_info) == BATTERY_TYPE_COUNT);

const enum battery_type DEFAULT_BATTERY_TYPE = BATTERY_SDI;

static int swelling_flag = -1;
static int prev_ac = -1;
static int chargeInterruptflag = -1;

#define Swelling_trigger_5 BIT(0)
#define Swelling_trigger_15 BIT(1)
#define Swelling_trigger_45 BIT(2)
#define Swelling_trigger_50 BIT(3)
#define Swelling_recovery_10 BIT(4)
#define Swelling_recovery_20 BIT(5)
#define Swelling_recovery_50 BIT(6)

int charger_profile_override(struct charge_state_data *curr)
{
	int bat_temp_c = (curr->batt.temperature - 2731) / 10;

	/*
	 *	start charge temp control
	 *
	 *	if bat_temp >= 45 or bat_temp <= 0 when adapter plugging in,
	 *	stop charge
	 *	if 0 < bat_temp < 45 when adapter plugging in, charge normal
	 */
	if (curr->ac != prev_ac) {
		if (curr->ac) {
			if ((bat_temp_c <= 0) || (bat_temp_c >= 45))
				chargeInterruptflag = 1;
		}
		prev_ac = curr->ac;
	}

	if (chargeInterruptflag) {
		curr->requested_current = 0;
		curr->requested_voltage = 0;
		curr->batt.flags &= ~BATT_FLAG_WANT_CHARGE;
		if (curr->state != ST_DISCHARGE)
			curr->state = ST_IDLE;
	}

	if ((bat_temp_c > 0) && (bat_temp_c < 45))
		chargeInterruptflag = 0;

/*
 *	battery swelling control
 *
 *	trigger condition			|	recovery condition
 *	bat_temp < 5,				|	batt_temp >= 10,
 *	bat_cell_voltage < 4.15		|	cv = (cell CV-50mv)*series
 *								|	= 8700mv
 *	cv = 4150mv*series = 8300mv	|	cc = FCC*C-rate*0.4 = 1464ma
 *	cc = FCC*C-rate*0.4			|
 *								|
 *	bat_temp < 15				|	batt_temp >= 20,
 *	bat_cell_voltage < 4.15		|	cv = (cell CV-50mv)*series
 *								|	= 8700mv
 *	cv = 4150mv*series = 8300mv	|	cc = FCC*C-rate*0.9 = 3294ma
 *	cc = FCC*C-rate*0.4= 1464ma	|
 *								|
 *	bat_temp >= 45				|	batt_temp < 43,
 *	cv = 4150mv*series = 8300mv	|	cv = (cell CV-50mv)*series
 *								|	= 8700mv
 *	cc = FCC*C-rate*0.45= 1647ma|	cc = FCC*C-rate*0.9 = 3294ma
 *								|
 *	bat_temp >= 50				|	batt_temp < 45,
 *	stop charge					|	recovery charge
 */
	if ((curr->state == ST_CHARGE) && !chargeInterruptflag) {
		if (curr->batt.voltage < 8300) {
			if (bat_temp_c < 5)
				swelling_flag = Swelling_trigger_5;
			else if (bat_temp_c < 15)
				swelling_flag = Swelling_trigger_15;

			if (bat_temp_c >= 50)
				swelling_flag = Swelling_trigger_50;
			else if (bat_temp_c >= 45) {
				if (!(swelling_flag & Swelling_trigger_50))
					swelling_flag = Swelling_trigger_45;
			}
		}

		if (swelling_flag) {
			if ((bat_temp_c >= 10) && (bat_temp_c < 20))
				swelling_flag = Swelling_recovery_10;
			else if ((bat_temp_c >= 20) && (bat_temp_c < 43))
				swelling_flag = Swelling_recovery_20;
			else if ((bat_temp_c >= 43) && (bat_temp_c < 45))
				swelling_flag = Swelling_recovery_50;
		} else
			curr->requested_voltage += 100;

		if (swelling_flag & Swelling_trigger_5) {
			curr->requested_voltage = 4150 * 2;
			curr->requested_current = 5230 * 0.7 * 0.4;
		} else if (swelling_flag & Swelling_trigger_15)
			curr->requested_current = 5230 * 0.7 * 0.4;
		else if (swelling_flag & Swelling_trigger_45) {
			curr->requested_voltage = 4150 * 2;
			curr->requested_current = 5230 * 0.7 * 0.45;
		} else if (swelling_flag & Swelling_trigger_50) {
			curr->requested_current = 0;
			curr->requested_voltage = 0;
			curr->batt.flags &= ~BATT_FLAG_WANT_CHARGE;
			if (curr->state != ST_DISCHARGE)
				curr->state = ST_IDLE;
		} else if (swelling_flag & Swelling_recovery_10)
			curr->requested_current = 5230 * 0.7 * 0.4;
		else if (swelling_flag &
			(Swelling_recovery_20|Swelling_recovery_50))
			curr->requested_current = 5230 * 0.7 * 0.9;
	} else
		swelling_flag = 0;

	return 0;
}

enum ec_status charger_profile_override_get_param(uint32_t param,
				uint32_t *value)
{
	return EC_RES_INVALID_PARAM;
}

enum ec_status charger_profile_override_set_param(uint32_t param,
				uint32_t value)
{
	return EC_RES_INVALID_PARAM;
}
