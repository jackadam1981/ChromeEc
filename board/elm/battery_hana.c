/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "battery_smart.h"
#include "system.h"
#include "util.h"

/* Shutdown mode parameter to write to manufacturer access register */
#define HANA_SB_SHIP_MODE_REG   0x34
#define HANA_SB_SHUTDOWN_DATA1  0x0000
#define HANA_SB_SHUTDOWN_DATA2  0x1000

#define BIRCH_SB_SHIP_MODE_REG  0x44
#define BIRCH_SB_SHUTDOWN_DATA  0x0010

static const struct battery_info hana_info = {
	.voltage_max = 12600,
	.voltage_normal = 11100,
	.voltage_min = 9100,
	/* Pre-charge values. */
	.precharge_current = 256, /* mA */

	.start_charging_min_c = 0,
	.start_charging_max_c = 50,
	.charging_min_c = 0,
	.charging_max_c = 60,
	.discharging_min_c = 0,
	.discharging_max_c = 60,
};

static const struct battery_info birch_info = {
	.voltage_max = 13050,
	.voltage_normal = 11400,
	.voltage_min = 9000,
	/* Pre-charge values. */
	.precharge_current = 300, /* mA */

	.start_charging_min_c = 0,
	.start_charging_max_c = 45,
	.charging_min_c = 0,
	.charging_max_c = 60,
	.discharging_min_c = 0,
	.discharging_max_c = 60,
};

const struct battery_info *battery_get_info(void)
{
	if (system_get_board_version() >= 7)
		return &birch_info;
	else
		return &hana_info;
}

int hana_cut_off_battery(void)
{
	int rv;

	/* Ship mode command must be sent twice to take effect */
	rv = sb_write(HANA_SB_SHIP_MODE_REG, HANA_SB_SHUTDOWN_DATA1);

	if (rv != EC_SUCCESS)
		return rv;

	return sb_write(HANA_SB_SHIP_MODE_REG, HANA_SB_SHUTDOWN_DATA2);
}

int birch_cut_off_battery(void)
{
	int rv;

	/* Ship mode command must be sent twice to take effect */
	rv = sb_write(BIRCH_SB_SHIP_MODE_REG, BIRCH_SB_SHUTDOWN_DATA);

	if (rv != EC_SUCCESS)
		return rv;

	return sb_write(BIRCH_SB_SHIP_MODE_REG, BIRCH_SB_SHUTDOWN_DATA);
}

int board_cut_off_battery(void)
{
	if (system_get_board_version() >= 7)
		return birch_cut_off_battery();
	else
		return hana_cut_off_battery();
}
