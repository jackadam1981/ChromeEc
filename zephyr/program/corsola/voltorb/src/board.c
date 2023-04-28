/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "common.h"
#include "dps.h"
#include "math_util.h"

#include <zephyr/logging/log.h>

#include <dt-bindings/battery.h>

#include "battery.h"
#include "battery_smart.h"
#include "console.h"
#include "timer.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_USB, format, ##args)

#define BATTERY_NO_RESPONSE_TIMEOUT (2000 * MSEC)

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

bool voltorb_is_more_efficient(int curr_mv, int prev_mv, int batt_mv,
			       int batt_mw, int input_mw)
{
	int batt_state;

	battery_status(&batt_state);

	/* Choose 15V PDO or higher when battery is full. */
	if ((batt_state & SB_STATUS_FULLY_CHARGED) && (curr_mv >= 15000) &&
	    (prev_mv < 15000 || curr_mv <= prev_mv)) {
		return true;
	} else {
		return ABS(curr_mv - batt_mv) < ABS(prev_mv - batt_mv);
	}
}

__override struct dps_config_t dps_config = {
	.k_less_pwr = 93,
	.k_more_pwr = 96,
	.k_sample = 1,
	.k_window = 3,
	.t_stable = 10 * SECOND,
	.t_check = 5 * SECOND,
	.is_more_efficient = &voltorb_is_more_efficient,
};

int board_battery_initialized(void)
{
	int status;
	uint64_t wait_timeout = get_time().val + BATTERY_NO_RESPONSE_TIMEOUT;

	while (get_time().val < wait_timeout) {
		/* Starting pinging battery */
		battery_status(&status);

		if ((status & STATUS_REMAINING_CAPACITY_ALARM) || 
			(status & STATUS_TERMINATE_CHARGE_ALARM)) {
			msleep(25); /* clock stretching could hold 25ms */
			continue;
		}

		CPRINTS("battery initialized status %x", status);
		return EC_SUCCESS;
	}

	CPRINTS("battery wait stable timeout");

	return EC_ERROR_TIMEOUT;
}
