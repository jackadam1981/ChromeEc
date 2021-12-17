/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>

#include "battery.h"
#include "battery_smart.h"
#include "emul/emul_smart_battery.h"
#include "hooks.h"
#include "power.h"
#include "stubs.h"

#define BATTERY_ORD DT_DEP_ORD(DT_NODELABEL(battery))

void test_set_chipset_to_s0(void)
{
	struct sbat_emul_bat_data *bat;
	struct i2c_emul *emul;

	emul = sbat_emul_get_ptr(BATTERY_ORD);
	bat = sbat_emul_get_bat_data(emul);

	/*
	 * Make sure that battery is in good condition to
	 * not trigger hibernate in charge_state_v2.c
	 */
	bat->cap = bat->full_cap * 3 / 4;
	bat->volt = battery_get_info()->voltage_normal;
	bat->design_mv = bat->volt;

	force_power_state(true, POWER_S0);

	hook_notify(HOOK_CHIPSET_RESUME);
	k_msleep(1);

	/* Stop forcing state and check if chipset is in correct state */
	force_power_state(false, POWER_S0);
	k_msleep(1);
	zassert_equal(POWER_S0, power_get_state(), NULL);
}
