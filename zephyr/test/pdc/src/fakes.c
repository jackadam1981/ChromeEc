/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"

static enum battery_present bp_val = BP_YES;
void set_battery_present(enum battery_present bp)
{
	bp_val = bp;
}

int battery_is_present(void)
{
	return bp_val;
}

int extpower_is_present(void)
{
	return 0;
}
