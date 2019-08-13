/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "console.h"
#include "double_tap.h"
#include "hooks.h"
#include "host_command.h"
#include "timer.h"

#define CPRINTS(format, args...) cprints(CC_MOTION_LID, format, ## args)

#ifdef CONFIG_DOUBLE_TAP_SWITCH
/* 1: triggered to wake up device, 0: otherwise */
static int double_tap_state;

int double_tap_get_state(void)
{
	return double_tap_state;
}

static void double_tap_reset_state(void)
{
	double_tap_state = 0;
	hook_notify(HOOK_DOUBLE_TAP_CHANGE);
	host_set_single_event(EC_HOST_EVENT_MODE_CHANGE);
}
DECLARE_DEFERRED(double_tap_reset_state);

void double_tap_set_state(void)
{
	if (double_tap_state == 1)
		return;

	double_tap_state = 1;
	hook_notify(HOOK_DOUBLE_TAP_CHANGE);
	host_set_single_event(EC_HOST_EVENT_MODE_CHANGE);
	hook_call_deferred(&double_tap_reset_state_data,
			   CONFIG_DOUBLE_TAP_RESET_US);
}
#endif
