/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "charger_override.h"

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>


struct charge_state_data curr;

#define CURRENT_ORIGINAL 5000
#define CURRENT_LIMIT 2000


/**
 * 25'C : 298.15'K -> normal charge
 * 55'C : 328.15'K -> normal charge
 * 70'C : 343.15'K -> current limit
 * 50'C : 323.15'K -> normal charge
 */
const int fake_temp[] = {298, 328, 343, 323};
const int aim_curr[] = {CURRENT_ORIGINAL, CURRENT_ORIGINAL,
			CURRENT_LIMIT,CURRENT_ORIGINAL};

ZTEST_USER(board_charger_override, test_board_charger_profile_override)
{
	int rv;

	curr.batt.flags |= BATT_FLAG_RESPONSIVE;
	curr.state = ST_CHARGE;

	for (int i = 0; i < ARRAY_SIZE(fake_temp); i++) {
		curr.requested_current = CURRENT_ORIGINAL;
		rv = board_charger_profile_override(&curr, fake_temp[i]);
		zassert_equal(EC_SUCCESS, rv, "function did not return success");
		zassert_equal(aim_curr[i], curr.requested_current, "requested_current not expected");
	}
}

ZTEST_SUITE(board_charger_override, NULL, NULL, NULL, NULL, NULL);
