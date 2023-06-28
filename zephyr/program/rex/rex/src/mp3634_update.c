/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Tune the MP2964 IMVP9.1 parameters for yaviks */
#include "charger.h"
#include "common.h"
#include "driver/mp3634.h"
#include "hooks.h"
#include "i2c.h"
const static struct mp3634_reg_val rail_a[] = {
	{ 0x00, 0X00 },
	{ 0x01, 0X00 },
	{ 0x02, 0X00 },
	{ 0x03, 0X00 },
	{ 0x04, 0X00 },
	{ 0x05, 0X00 },
	{ 0x06, 0X00 },
	{ 0x07, 0X00 },
	{ 0x08, 0X00 },
	{ 0x09, 0X00 },
	{ 0x0A, 0X00 },
	{ 0x10, 0X00 },
	{ 0x11, 0X00 },
	{ 0x12, 0X00 },
	{ 0x13, 0X00 },
	{ 0x14, 0X00 },
	{ 0x15, 0X00 },
	{ 0x16, 0X00 },
	{ 0x17, 0X00 },
	{ 0x18, 0X00 },
	{ 0x19, 0X00 },
	{ 0x1A, 0X00 },
	{ 0x1B, 0X00 },
	{ 0x1C, 0X00 },
	{ 0x1D, 0X00 },
	{ 0x1E, 0X00 },
	{ 0x1F, 0X00 },
	{ 0x20, 0X00 },
	{ 0x21, 0X00 },
	{ 0x22, 0X00 },
	{ 0x23, 0X00 },
	{ 0x24, 0X00 },
	{ 0x25, 0X00 },
	{ 0x26, 0X00 },
	{ 0x27, 0X00 },
	{ 0x28, 0X00 },
	{ 0x29, 0X00 },
	{ 0x2A, 0X00 },
	{ 0x2B, 0X00 },
	{ 0x2C, 0X00 },
	{ 0x2D, 0X00 },
	{ 0x2E, 0X00 },
	{ 0x2F, 0X00 },
	{ 0x30, 0X00 },
	{ 0x31, 0X00 },
	{ 0x32, 0X00 },
	{ 0x33, 0X00 },
	{ 0x34, 0X00 },
	{ 0x35, 0X00 },
	{ 0x36, 0X00 },
	{ 0x37, 0X00 },
	{ 0x38, 0X00 },
	{ 0x39, 0X00 },
	{ 0x3A, 0X00 },
	{ 0x3B, 0X00 },
	{ 0x3C, 0X00 },
	{ 0x3D, 0X00 },
	{ 0x3E, 0X00 },
	{ 0x3F, 0X00 },
	{ 0x70, 0X00 },
	{ 0x71, 0X00 },
	{ 0x72, 0X00 },
	{ 0x73, 0X00 },
	{ 0x74, 0X00 },
	{ 0x7A, 0X00 },
	{ 0x7B, 0X00 },
	{ 0x7C, 0X00 },
	{ 0x7D, 0X00 },
};
const static struct mp3634_reg_val rail_b[] = {
	{ 0x90, 0X00 },
	{ 0x91, 0X00 },
	{ 0x92, 0X00 },
	{ 0x93, 0X00 },
	{ 0x94, 0X00 },
	{ 0x95, 0X00 },
	{ 0x96, 0X00 },
	{ 0x97, 0X00 },
	{ 0x98, 0X00 },
	{ 0x99, 0X00 },
	{ 0x9A, 0X00 },
	{ 0x9B, 0X00 },
	{ 0x9C, 0X00 },
	{ 0x9D, 0X00 },
	{ 0x9E, 0X00 },
};
static void mp3634_on_startup(void)
{
	static int chip_updated;
	int status;

	if (chip_updated)
		return;
	chip_updated = 1;
	ccprintf("%s: attempting to tune PMIC\n", __func__);
	status = mp3634_tune(rail_a, ARRAY_SIZE(rail_a),
			     rail_b, ARRAY_SIZE(rail_b));

	ccprintf("%s: PMIC update done\n", __func__);
	if (status != EC_SUCCESS)
		ccprintf("%s: could not update all settings\n", __func__);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, mp3634_on_startup, HOOK_PRIO_FIRST);

