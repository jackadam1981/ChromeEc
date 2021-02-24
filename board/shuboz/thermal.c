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

static uint16_t current_table[] = {
	2200,
	1800,
	1700,
	1600,
};
#define NUM_CURRENT_LEVELS ARRAY_SIZE(current_table)

int current_level;

__override int board_max_current_limit(int current_requested)
{
	int current_max;

	if (current_level == 0)
		current_max = current_requested;
	else {
		if (current_requested > current_table[current_level-1])
			current_max = current_table[current_level - 1];
		else
			current_max = current_requested;
	}
	return current_max;
}

/* Called by hook task every hook second (1 sec) */
static void current_update(void)
{
	int t, temp;
	static int Uptime;
	static int Dntime;

	temp_sensor_read(0, &t);
	temp = K_TO_C(t);

	if (temp > 54) {
		Dntime = 0;

		if (Uptime < 5)
			Uptime++;
		else {
			Uptime = 0;
			current_level++;
		}
	} else if (current_level != 0 && temp < 54) {
		Uptime = 0;

		if (Dntime < 5)
			Dntime++;
		else {
			Dntime = 0;
			current_level--;
		}
	} else {
		Uptime = 0;
		Dntime = 0;
	}
	if (current_level < 0)
		current_level = 0;
	else if (current_level > NUM_CURRENT_LEVELS)
		current_level = NUM_CURRENT_LEVELS;
}
DECLARE_HOOK(HOOK_SECOND, current_update, HOOK_PRIO_DEFAULT);
