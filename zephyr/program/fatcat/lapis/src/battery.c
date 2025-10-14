/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"

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
			retry_cnt++;

			if (retry_cnt > 100) {
				printk("--retry_timeout:%d\n", retry_cnt);
				retry_cnt = 100;
				return BP_NO;
			} else {
				printk("--retrying:%d\n", retry_cnt);
				return BP_YES;
			}
		}
	}

	printk("--batt_OK\n");

	retry_cnt = 0;

	return BP_YES;
}
