/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "system.h"
#include "timer.h"

#include <zephyr/logging/log.h>

#include <ap_power/ap_power.h>
#include <power_signals.h>

LOG_MODULE_DECLARE(ovis, CONFIG_OVIS_LOG_LEVEL);

void reboot_for_wrong_power_signal(void)
{
	/*
	 * Currently there's chance that vw PWL_SLP_S5 and PWL_SLP_S4
	 * failed to be cleared through eSPI when AP do global reset,
	 * which confuses EC that AP is still in S5 and then set it back to
	 * G3. When the issue occurs, real pin PWL_SLP_S3 will be cleared.
	 * Therefore, check for this state after shutdown 5 sec (it takes about
	 * 4 sec from shutdown to power on to S0 when AP doing global reset, set
	 * 5 sec delay should be fine) and reset EC in such condition.
	 * See b:384085356 for more details.
	 */
	if (power_signal_get(PWR_SLP_S5) && power_signal_get(PWR_SLP_S4) &&
	    !power_signal_get(PWR_SLP_S3)) {
		LOG_WRN("VW PWR_SLP_S5 and PWR_SLP_S4 failed to cleared! Rebooting...");
		system_reset(SYSTEM_RESET_MANUALLY_TRIGGERED);
	}
}
DECLARE_DEFERRED(reboot_for_wrong_power_signal);

void board_chipset_shutdown(void)
{
	hook_call_deferred(&reboot_for_wrong_power_signal_data, 5 * SECOND);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_chipset_shutdown, HOOK_PRIO_DEFAULT);
