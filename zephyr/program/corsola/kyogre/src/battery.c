/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery_fuel_gauge.h"
#include "battery_smart.h"
#include "hooks.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

static char str[32];
static uint32_t sb_data_01;
static uint32_t sb_data_02;
static uint32_t sb_data_03;

#define SB_LOT 0x31
#define SB_CMD01 0x96
#define SB_CMD02 0x97
#define SB_CMD03 0x98
#define SB_DAT01 0x0B68
#define SB_DAT02 0x0190
#define SB_DAT03 0x000A

#define BATTERY_TYPE_COUNT 2

void board_battery(void)
{
	char device_name[32];
	int i;

	if (battery_device_name(device_name, sizeof(device_name)))
		/* No responce from battery, exit the process */
		return;

	for (i = 0; i <= BATTERY_TYPE_COUNT; i++) {
		if (i == BATTERY_TYPE_COUNT)
			/* Device name do not match, exit the process  */
			return;
		if (!strncasecmp(device_name, "CP856931", 8))
			/* Device name match, continue the process */
			break;
		if (!strncasecmp(device_name, "CP856932", 8))
			/* Device name match, continue the process */
			break;
	}

	/* Read/Write the board battery data */
	if (sb_read_string(SB_LOT, str, sizeof(str)))
		return;
	if (sb_write(SB_CMD01, SB_DAT01))
		return;
	if (sb_read(SB_CMD01, &sb_data_01))
		return;
	if (sb_write(SB_CMD02, SB_DAT02))
		return;
	if (sb_read(SB_CMD02, &sb_data_02))
		return;
	if (sb_write(SB_CMD03, SB_DAT03))
		return;
	if (sb_read(SB_CMD03, &sb_data_03))
		return;
}
DECLARE_HOOK(HOOK_INIT, board_battery, HOOK_PRIO_BATTERY_INIT);

static void output_board_battery(void)
{
	/* output the board battery data */
	CPRINTS("SB_LOT : %s", str);
	CPRINTS("SB_DAT01 : %04x", sb_data_01);
	CPRINTS("SB_DAT02 : %04x", sb_data_02);
	CPRINTS("SB_DAT03 : %04x", sb_data_03);
}
DECLARE_DEFERRED(output_board_battery);

static void startup_board_battery(void)
{
	/* Set the timer */
	hook_call_deferred(&output_board_battery_data, 10000 * MSEC);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, startup_board_battery, HOOK_PRIO_DEFAULT);
