/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery_fuel_gauge.h"
#include "battery_smart.h"
#include "builtin/assert.h"
#include "console.h"
#include "cros_board_info.h"
#include "hooks.h"
#include "i2c.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

#define RETRY_NUMBER 10

int board_cut_off_battery(void)
{
	const struct board_batt_params *params = get_batt_params();
	int rv;
	int retry_number = RETRY_NUMBER;

	/* If battery is unknown can't send ship mode command */
	if (!params)
		return EC_RES_ERROR;

	do {
		if (params->fuel_gauge.flags & FUEL_GAUGE_FLAG_WRITE_BLOCK)
			rv = cut_off_battery_block_write(
				&params->fuel_gauge.ship_mode);
		else
			rv = cut_off_battery_sb_write(
				&params->fuel_gauge.ship_mode);

		if (rv == EC_RES_SUCCESS) {
			break;
		} else {
			CPRINTS("Battery cutoff failed, retry: %d rv: %d",
				retry_number, rv);
			watchdog_reload();
			k_busy_wait(500 * USEC_PER_MSEC);
		}
	} while (--retry_number);

	return rv ? EC_RES_ERROR : EC_RES_SUCCESS;
}
