/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"
#include "charger.h"
#include "hooks.h"

enum battery_present battery_is_present(void)
{
	static int retry_cnt;
	int manufacture_date;

	if (sb_read(SB_MANUFACTURE_DATE, &manufacture_date)) {
		/* Require 2 consecutive failures before declaring the
		 * battery missing.
		 */
		k_msleep(25);
		if (sb_read(SB_MANUFACTURE_DATE, &manufacture_date)) {
			if (retry_cnt > 100) {
				return BP_NO;
			} else {
				retry_cnt++;

				return BP_YES;
			}
		}
	}

	retry_cnt = 0;

	return BP_YES;
}

static void set_chg_reg_custom(void)
{
	charger_set_frequency(808);
}
DECLARE_HOOK(HOOK_INIT, set_chg_reg_custom, HOOK_PRIO_POST_BATTERY_INIT + 1);
