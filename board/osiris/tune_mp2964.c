/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Tune the MP2964 IMVP9.1 parameters for brya */

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "hooks.h"
#include "mp2964.h"

const static struct mp2964_reg_val rail_a[] = {
	{.reg = 0x29, .val = 0x0003 },
	{.reg = 0x2C, .val = 0x03A1 },
	{.reg = 0x38, .val = 0x007D },
	{.reg = 0x3D, .val = 0x1BC3 },
	{.reg = 0x48, .val = 0x01DD },
	{.reg = 0x53, .val = 0x006D },
	{.reg = 0x60, .val = 0x90B0 },
	{.reg = 0x62, .val = 0x0CAF },
	{.reg = 0x93, .val = 0x006E },
	{.reg = 0x94, .val = 0x0000 },
	{.reg = 0xB0, .val = 0x10C4 },
	{.reg = 0xB3, .val = 0x14C0 },
	{.reg = 0xC1, .val = 0x25B0 },
	{.reg = 0xE0, .val = 0x0063 },
	{.reg = 0xE8, .val = 0x0093 },
	{.reg = 0xE9, .val = 0x0093 },
	{.reg = 0xEA, .val = 0x0093 },
	{.reg = 0xEB, .val = 0x0093 },
	{.reg = 0xEF, .val = 0x00A9 },
	{.reg = 0xF0, .val = 0x00A9 },
};
const static struct mp2964_reg_val rail_b[] = {
	{.reg = 0x22, .val = 0x0EEE },
	{.reg = 0x29, .val = 0x0002 },
	{.reg = 0x2C, .val = 0x0358 },
	{.reg = 0x38, .val = 0x0042 },
	{.reg = 0x3D, .val = 0x1BC3 },
	{.reg = 0x48, .val = 0x0100 },
	{.reg = 0x53, .val = 0x0032 },
	{.reg = 0x60, .val = 0x42B0 },
	{.reg = 0x62, .val = 0x0CA0 },
	{.reg = 0xB0, .val = 0x10A4 },
};

static void mp2964_on_startup(void)
{
	static int chip_updated;
	uint8_t board_id;
	int status;

	if (chip_updated)
		return;

	board_id = get_board_id();

	if (board_id != 0 && board_id != 1)
		return;

	chip_updated = 1;

	ccprintf("%s: attempting to tune PMIC\n", __func__);

	status = mp2964_tune(rail_a, ARRAY_SIZE(rail_a),
			     rail_b, ARRAY_SIZE(rail_b));
	if (status != EC_SUCCESS)
		ccprintf("%s: could not update all settings\n", __func__);
}

DECLARE_HOOK(HOOK_CHIPSET_STARTUP, mp2964_on_startup,
	     HOOK_PRIO_FIRST);
