/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "util.h"

#define BATT_68WH_1_C 5830
#define BATT_56WH_1_C 4840
#define BATT_68WH_0_6_5_C 3790
#define BATT_56WH_0_6_5_C 3146
#define BATT_68WH_0_7_C ((BATT_68WH_1_C * 7) / 10)
#define BATT_56WH_0_7_C ((BATT_56WH_1_C * 7) / 10)

int charger_profile_override(struct charge_state_data *curr)
{
	if (curr->batt.desired_current == BATT_68WH_1_C) {
		/* 68Wh battery charging at 1C, reduce to 0.7C */
		curr->requested_current =
			MIN(curr->requested_current, BATT_68WH_0_7_C);
	} else if (curr->batt.desired_current == BATT_56WH_1_C) {
		/* 56Wh battery charging at 1C, reduce to 0.7C */
		curr->requested_current =
			MIN(curr->requested_current, BATT_56WH_0_7_C);
	}

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
