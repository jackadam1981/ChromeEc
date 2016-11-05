/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "charge_state.h"
#include "charger_profile.h"
#include "console.h"
#include "ec_commands.h"
#include "i2c.h"
#include "util.h"

/* Battery temperature ranges in degrees C */
static const struct battery_info info = {
	/* Design voltage */
	.voltage_max    = 4350,
	.voltage_normal = 3800,
	.voltage_min    = 2800,
	/* Pre-charge current: I <= 0.01C */
	.precharge_current  = 64,  /* mA */
	/* Operational temperature range */
	.start_charging_min_c = 5,
	.start_charging_max_c = 48,
	.charging_min_c       = 5,
	.charging_max_c       = 48,
	.discharging_min_c    = -20,
	.discharging_max_c    = 60,
};

const struct battery_info *battery_get_info(void)
{
	return &info;
}

int board_cut_off_battery(void)
{
	/* Write SET_SHUTDOWN(0x13) to CTRL(0x00) */
	return i2c_write16(I2C_PORT_BATTERY, 0xaa, 0x0, 0x13);
}

#ifdef CONFIG_CHARGER_PROFILE_OVERRIDE
const struct fast_charge_profile fast_charge_info[] = {
	/* < 10C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(9),
		.curr_high_vtg = 900,
		.curr_low_vtg = 900,
		.voltage = 4350,
	},
	/* 10-15C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(15),
		.curr_high_vtg = 2700,
		.curr_low_vtg = 2700,
		.voltage = 4350,
	},
	/* 15-23C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(23),
		.curr_high_vtg = 4500,
		.curr_low_vtg = 6300,
		.voltage = 4350,
	},
	/* 23-45C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(45),
		.curr_high_vtg = 4500,
		.curr_low_vtg = 9000,
		.voltage = 4350,
	},
	/* > 45C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(0xFFFF),
		.curr_high_vtg = 4500,
		.curr_low_vtg = 4500,
		.voltage = 4150,
	},
};

struct fast_charge_config fast_charge_config_info = {
	.temp_ranges = ARRAY_SIZE(fast_charge_info),
	.temp_last_range = 2,
	.vtg_low_limit = 4130,
	.vtg_high_limit = 4150,
};
#endif	/* CONFIG_CHARGER_PROFILE_OVERRIDE */
