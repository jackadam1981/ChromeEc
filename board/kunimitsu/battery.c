/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "battery_smart.h"
#include "util.h"

/* Shutdown mode parameter to write to manufacturer access register */
#define SB_SHUTDOWN_DATA	0x0010

static const struct battery_info info = {
	.voltage_max = 8700,/* mV */
	.voltage_normal = 7600,
	.voltage_min = 6000,
	.precharge_current = 150,/* mA */
	.start_charging_min_c = 0,
	.start_charging_max_c = 45,
	.charging_min_c = 0,
	.charging_max_c = 45,
	.discharging_min_c = -20,
	.discharging_max_c = 60,
};

const struct battery_info *battery_get_info(void)
{
	return &info;
}

int board_cut_off_battery(void)
{
	int rv;

	/* Ship mode command must be sent twice to take effect */
	rv = sb_write(SB_MANUFACTURER_ACCESS, SB_SHUTDOWN_DATA);

	if (rv != EC_SUCCESS)
		return rv;

	return sb_write(SB_MANUFACTURER_ACCESS, SB_SHUTDOWN_DATA);
}

int board_was_battery_cut_off(void)
{
	int batt_status;
	int bat_cut_off = 0;

	/*
	 * FETs are turned off after Power Shutdown time.
	 * The device will wake up when a voltage is applied to PACK.
	 * Battery status will be inactive until it is initialized.
	 *
	 * Make sure battery status is implemented and the I2C transactions
	 * are success to figure out if it is a working battery.
	 */
	if ((battery_is_present() == BP_YES) &&
		!battery_status(&batt_status))
		if (!(batt_status & STATUS_INITIALIZED))
			bat_cut_off = 1;

	return bat_cut_off;
}
