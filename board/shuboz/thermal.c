/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "extpower.h"
#include "fan.h"
#include "hooks.h"
#include "host_command.h"
#include "temp_sensor.h"
#include "thermal.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_THERMAL, outstr)
#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ## args)

__override int board_max_current_limit(int current_requested)
{
	struct batt_params batt;

	battery_get_params(&batt);

	if (batt.state_of_charge < 30)
		return current_requested;
	else if (batt.state_of_charge < 40)
		return 2200;
	else if (batt.state_of_charge < 50)
		return 2000;
	else if (batt.state_of_charge < 60)
		return 1800;
	else if (batt.state_of_charge < 70)
		return 1600;
	else if (batt.state_of_charge < 80)
		return 1300;
	else if (batt.state_of_charge < 90)
		return 1000;
	else
		return current_requested;

	return current_requested;
}


