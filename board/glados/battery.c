/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "battery_smart.h"
#include "charge_state.h"
#include "charger_profile.h"
#include "console.h"
#include "ec_commands.h"
#include "i2c.h"
#include "util.h"

/* Shutdown mode parameter to write to manufacturer access register */
#define PARAM_CUT_OFF_LOW  0x10
#define PARAM_CUT_OFF_HIGH 0x00

/* Battery info for BQ40Z55 */
static const struct battery_info info = {
	.voltage_max = 8700,        /* mV */
	.voltage_normal = 7600,
	.voltage_min = 6000,
	.precharge_current = 256,   /* mA */
	.start_charging_min_c = 0,
	.start_charging_max_c = 46,
	.charging_min_c = 0,
	.charging_max_c = 60,
	.discharging_min_c = 0,
	.discharging_max_c = 60,
};

const struct battery_info *battery_get_info(void)
{
	return &info;
}

int board_cut_off_battery(void)
{
	int rv;
	uint8_t buf[3];

	/* Ship mode command must be sent twice to take effect */
	buf[0] = SB_MANUFACTURER_ACCESS & 0xff;
	buf[1] = PARAM_CUT_OFF_LOW;
	buf[2] = PARAM_CUT_OFF_HIGH;

	i2c_lock(I2C_PORT_BATTERY, 1);
	rv = i2c_xfer(I2C_PORT_BATTERY, BATTERY_ADDR, buf, 3, NULL, 0,
		      I2C_XFER_SINGLE);
	rv |= i2c_xfer(I2C_PORT_BATTERY, BATTERY_ADDR, buf, 3, NULL, 0,
		       I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_BATTERY, 0);

	return rv;
}

#ifdef CONFIG_CHARGER_PROFILE_OVERRIDE
const struct fast_charge_profile fast_charge_info[] = {
	/* < 10C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(9),
		.curr_high_vtg = 486,
		.curr_low_vtg = 486,
		.voltage = 8700,
	},
	/* 10-15C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(15),
		.curr_high_vtg = 1458,
		.curr_low_vtg = 1458,
		.voltage = 8700,
	},
	/* 15-23C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(23),
		.curr_high_vtg = 2430,
		.curr_low_vtg = 3402,
		.voltage = 8700,
	},
	/* 23-45C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(45),
		.curr_high_vtg = 2430,
		.curr_low_vtg = 4860,
		.voltage = 8700,
	},
	/* > 45C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(0xFFFF),
		.curr_high_vtg = 2430,
		.curr_low_vtg = 2430,
		.voltage = 8300,
	},
};

struct fast_charge_config fast_charge_config_info = {
	.temp_ranges = ARRAY_SIZE(fast_charge_info),
	.temp_last_range = 2,
	.vtg_low_limit = 8200,
	.vtg_high_limit = 8300,
};

#endif	/* CONFIG_CHARGER_PROFILE_OVERRIDE */
