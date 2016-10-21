/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common LPC module for Chrome EC
 */

#include "chipset.h"
#include "ec_commands.h"
#include "lpc.h"

#ifdef CONFIG_POWER_S0IX
static void lpc_clear_host_events(void)
{
	while (lpc_query_host_event_state() != 0)
		;
}

/*
 * In AP S0 -> S3 & S0ix transitions,
 * the chipset_suspend is called.
 *
 * The chipset_in_state(CHIPSET_STATE_STANDBY | CHIPSET_STATE_ON)
 * is used to detect the S0ix transiton.
 *
 * During S0ix entry, the wake mask for lid open is enabled.
 */
void lpc_enable_wake_mask_for_lid_open(void)
{
	if (chipset_in_state(CHIPSET_STATE_STANDBY | CHIPSET_STATE_ON)) {
		uint32_t mask;

		mask = lpc_get_host_event_mask(LPC_HOST_EVENT_WAKE) |
			EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_OPEN);

		lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, mask);
	}
}

/*
 * In AP S0ix & S3 -> S0 transitions,
 * the chipset_resume hook is called.
 *
 * During S0ix exit, the wake mask for lid open is disabled.
 * All pending events are cleared
 */
void lpc_disable_wake_mask_for_lid_open(void)
{
	if (chipset_in_state(CHIPSET_STATE_STANDBY | CHIPSET_STATE_ON)) {
		lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, 0);
		lpc_clear_host_events();
	}
}
#endif
