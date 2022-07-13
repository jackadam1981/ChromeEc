/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef CONFIG_AP_PWRSEQ_S0IX_ERROR_RECOVERY

#include <ap_power_host_sleep.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>

void power_chipset_handle_sleep_hang(enum sleep_hang_type hang_type)
{
	/*
	 * Wake up the AP so they don't just chill in a non-suspended state and
	 * burn power. Overload a vaguely related event bit since event bits are
	 * at a premium. If the system never entered S0ix, then manually set the
	 * wake mask to pretend it did, so that the hang detect event wakes the
	 * system.
	 */
	if (pwr_sm_get_state() == SYS_POWER_STATE_S0) {
		host_event_t sleep_wake_mask;

		ap_power_get_lazy_wake_mask(SYS_POWER_STATE_S0ix,
					    &sleep_wake_mask);
		lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, sleep_wake_mask);
	}

	ccprintf("Warning: Detected sleep hang! Waking host up!");
	host_set_single_event(EC_HOST_EVENT_HANG_DETECT);
}

#endif /* CONFIG_AP_PWRSEQ_S0IX_ERROR_RECOVERY */
