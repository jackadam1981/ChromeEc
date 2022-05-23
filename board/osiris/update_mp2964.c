/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Update the MP2964 reg for osiris */

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "cros_board_info.h"
#include "hooks.h"
#include "mp2964.h"
#include <stdint.h>

/* update data table of page 0 */
static const struct mp2964_reg_val data_table_p0[] = {
	{.reg = 0x29 .val = 0x0003 },
	{.reg = 0x2C .val = 0x03A1 },
	{.reg = 0x38 .val = 0x007D },
	{.reg = 0x3D .val = 0x1BC3 },
	{.reg = 0x48 .val = 0x01DD },
	{.reg = 0x53 .val = 0x006D },
	{.reg = 0x60 .val = 0x90B0 },
	{.reg = 0x62 .val = 0x0CAF },
	{.reg = 0x93 .val = 0x006E },
	{.reg = 0x94 .val = 0x0000 },
	{.reg = 0xB0 .val = 0x10C4 },
	{.reg = 0xB3 .val = 0x14C0 },
	{.reg = 0xC1 .val = 0x25B0 },
	{.reg = 0xE0 .val = 0x0063 },
	{.reg = 0xE8 .val = 0x0093 },
	{.reg = 0xE9 .val = 0x0093 },
	{.reg = 0xEA .val = 0x0093 },
	{.reg = 0xEB .val = 0x0093 },
	{.reg = 0xEF .val = 0x00A9 },
	{.reg = 0xF0 .val = 0x00A9 },
};
#define NUM_PAGE0 ARRAY_SIZE(data_table_p0)

/* update data table of page 1 */
static const struct mp2964_reg_val data_table_p1[] = {
	{.reg = 0x22 .val = 0x0EEE },
	{.reg = 0x29 .val = 0x0002 },
	{.reg = 0x2C .val = 0x0358 },
	{.reg = 0x38 .val = 0x0042 },
	{.reg = 0x3D .val = 0x1BC3 },
	{.reg = 0x48 .val = 0x0100 },
	{.reg = 0x53 .val = 0x0032 },
	{.reg = 0x60 .val = 0x42B0 },
	{.reg = 0x62 .val = 0x0CA0 },
	{.reg = 0xB0 .val = 0x10A4 },
};
#define NUM_PAGE1 ARRAY_SIZE(data_table_p1)

static void update_mp2964(void)
{
	static int chip_updated;
	int i, rv, version;

	if (cbi_get_board_version(&version) != EC_SUCCESS ||
	    version > UINT8_MAX) {
		CPRINTS("%s: Read Board version failed", __func__);
		return;
	}

	/* update board version 1 */
	if (version != 1 || chip_updated ) {
		CPRINTS("%s: Not need to update", __func__);
		return;
	}

	CPRINTS("%s: Start to update", __func__);

	/* update page 0 */

	mp2964_select_page(REG_PAGE_0);

	for (i = 0; i < NUM_PAGE0; i++) {
		mp2964_write16(data_table_p0.reg, data_table_p0.val);
	}

	/* update page 1 */

	mp2964_select_page(REG_PAGE_1);

	for (i = 0; i < NUM_PAGE1; i++) {
		mp2964_write16(data_table_p1.reg, data_table_p1.val);
	}

	mp2964_store_user_all();

	chip_updated = 1;

	ccprintf("%s: Done!\n", __func__);
	

}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, update_mp2964,
	     HOOK_PRIO_FIRST);
