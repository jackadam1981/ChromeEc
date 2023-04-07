/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "common.h"
#include "console.h"
#include "dps.h"
#include "math_util.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

#define CPRINTF(format, args...) cprintf(CC_USBPD, "DPS " format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, "DPS " format, ##args)

bool voltorb_is_more_efficient(int curr_mv, int prev_mv, int batt_mv,
			       int batt_mw, int input_mw)
{
	/* Choose 15v PDO when battery is full. */
	if ((charge_get_state() == PWR_STATE_CHARGE_NEAR_FULL) &&
	    (curr_mv >= 15000) && (curr_mv != prev_mv)) {
		CPRINTS("voltorb choose 15v");
		return true;
	} else
		return ABS(curr_mv - batt_mv) < ABS(prev_mv - batt_mv);
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
