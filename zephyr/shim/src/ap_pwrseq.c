/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <ap_power/ap_power_interface.h>
#include "hooks.h"

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

#if defined(CONFIG_CHARGER)
static void hook_battery_soc_change(void)
{
	static bool cache_is_ok_to_power_up;
	bool is_ok_to_power_up = ap_power_is_ok_to_power_up();

	if (is_ok_to_power_up == cache_is_ok_to_power_up) {
		/* No change respect to previous state, return */
		return;
	}

	cache_is_ok_to_power_up = is_ok_to_power_up;

	if (is_ok_to_power_up) {
		ap_power_exit_hardoff();
	}
	LOG_INF("Battery is %s to boot AP!",
		 is_ok_to_power_up ? "OK" : "NOT OK");
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, hook_battery_soc_change,
	     HOOK_PRIO_DEFAULT);
#endif
