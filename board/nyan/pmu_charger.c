/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Nyan board-specific pmu/charger routines */

#include "console.h"
#include "smart_battery.h"

#define BATTERY_AP_OFF_LEVEL 1

#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)

int pmu_shutdown(void)
{
	/* no EC controlled pmu */
	CPRINTF("[pmu] No pmu shutdown supported !!!\n");
	return EC_ERROR_UNKNOWN;
}

int charge_keep_power_off(void)
{
	int charge;

	if (BATTERY_AP_OFF_LEVEL == 0)
		return 0;

	if (battery_remaining_capacity(&charge)) {
		CPRINTF("[charger] read battery capacity failed\n");
		return 0;
	}

	return charge <= BATTERY_AP_OFF_LEVEL;
}
