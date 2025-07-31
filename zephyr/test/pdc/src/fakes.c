/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "fakes.h"

#include <zephyr/fff.h>

DEFINE_FAKE_VALUE_FUNC(int, battery_design_voltage, uint32_t *);
DEFINE_FAKE_VALUE_FUNC(int, battery_remaining_capacity, uint32_t *);
DEFINE_FAKE_VALUE_FUNC(int, battery_status, uint32_t *);
DEFINE_FAKE_VALUE_FUNC(int, battery_design_capacity, uint32_t *);
DEFINE_FAKE_VALUE_FUNC(int, battery_full_charge_capacity, uint32_t *);

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
