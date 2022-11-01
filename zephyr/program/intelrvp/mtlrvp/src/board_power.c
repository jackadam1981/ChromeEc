/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <ap_power_override_functions.h>
#include <power_signals.h>
#include <x86_power_signals.h>

#include "ap_power/ap_pwrseq_sm.h"

LOG_MODULE_DECLARE(ap_pwrseq, LOG_LEVEL_INF);

#if CONFIG_X86_NON_DSX_PWRSEQ_MTL
#define X86_NON_DSX_MTL_FORCE_SHUTDOWN_TO_MS 50

void board_ap_power_force_shutdown(void)
{
	power_signal_set(PWR_EC_PCH_RSMRST, 0);
	power_signal_set(PWR_EN_PP3300_A, 0);
}

int board_ap_power_action_g3_entry(void *data)
{
	int timeout_ms = X86_NON_DSX_MTL_FORCE_SHUTDOWN_TO_MS;

	/* Turn off PCH_RMSRST to meet tPCH12 */
	power_signal_set(PWR_EC_PCH_RSMRST, 0);

	/* Turn off PRIM load switch. */
	power_signal_set(PWR_EN_PP3300_A, 0);
	/* This timer is for checking signal state */
	while (power_signal_get(PWR_RSMRST) && (timeout_ms > 0)) {
		k_msleep(1);
		timeout_ms--;
	};

	return 0;
}

static int board_ap_power_action_g3_run(void *data)
{
	if (IS_EVENT_SET(data, AP_PWRSEQ_EVENT_POWER_STARTUP)) {
		/* Turn on the PP3300_PRIM rail. */
		power_signal_set(PWR_EN_PP3300_A, 1);
	}

	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_G3,
			  board_ap_power_action_g3_entry,
			  board_ap_power_action_g3_run,
			  NULL)

static int board_ap_power_action_s3_run(void *data)
{
	int all_sys_pwrgd_in;

	if (power_signal_get(PWR_EC_PCH_SYS_PWROK) == 0) {
		k_msleep(AP_PWRSEQ_DT_VALUE(sys_pwrok_delay));
	}

	all_sys_pwrgd_in = power_signal_get(PWR_ALL_SYS_PWRGD);
	/* Loop through all PWROK signals defined by the board */
	if (all_sys_pwrgd_in == 0) {
		LOG_ERR("PG_EC_ALL_SYS_PWRGD deasserted, "
			"shutting AP off!");
		return 1;
	}

	power_signal_set(PWR_EC_PCH_SYS_PWROK, all_sys_pwrgd_in);
	/* PCH_PWROK is set to combined result of ALL_SYS_PWRGD and SLP_S3 */
	power_signal_set(PWR_PCH_PWROK,
			 all_sys_pwrgd_in && !power_signal_get(PWR_SLP_S3));
	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_S3,
			  NULL,
			  board_ap_power_action_s3_run,
			  NULL)
#endif /* CONFIG_X86_NON_DSX_PWRSEQ_MTL */
