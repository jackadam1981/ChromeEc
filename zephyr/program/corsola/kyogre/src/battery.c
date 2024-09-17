/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery_fuel_gauge.h"
#include "battery_smart.h"
#include "hooks.h"

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

void board_battery_type(void)
{
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
DECLARE_HOOK(HOOK_INIT, board_battery_type, HOOK_PRIO_BATTERY_INIT);

static void output_board_battery_type(void)
{
	CPRINTS("SB_LOT : %s", str);
	CPRINTS("SB_DAT01 : %04x", sb_data_01);
	CPRINTS("SB_DAT02 : %04x", sb_data_02);
	CPRINTS("SB_DAT03 : %04x", sb_data_03);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, output_board_battery_type, HOOK_PRIO_DEFAULT);
