/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Stop charging battery if not finished after a set duration.
 */

#include "charge_state.h"
#include "console.h"
#include "hooks.h"

#ifndef CHARGE_TIMEOUT
#define CHARGE_TIMEOUT (10 * HOUR)
#endif

static timestamp_t charge_start_time;

static void charge_state_change_hook(void)
{
	int state = charge_get_state();

	if (state == PWR_STATE_CHARGE)
		charge_start_time = get_time();
}
DECLARE_HOOK(HOOK_CHARGE_STATE_CHANGE, charge_state_change_hook,
	     HOOK_PRIO_DEFAULT);

static void charge_timeout_tick_hook(void)
{
	timestamp_t now = get_time();
	int state = charge_get_state();

	if (state != PWR_STATE_CHARGE)
		return;

	if (charge_start_time.val + CHARGE_TIMEOUT > now.val)
		return;

	ccprintf("[%T Charging Error: Charge timed out.]\n");
	charge_force_idle(1);
}
DECLARE_HOOK(HOOK_SECOND, charge_timeout_tick_hook, HOOK_PRIO_DEFAULT);
