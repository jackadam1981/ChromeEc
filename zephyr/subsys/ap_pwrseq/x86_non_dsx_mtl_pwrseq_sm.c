/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <x86_non_dsx_common_pwrseq_sm_handler.h>

#include "ap_power/ap_pwrseq_sm.h"

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

static void x86_non_dsx_mtl_s0_exit(void *data)
{
	power_signal_set(PWR_PCH_PWROK, 0);
	power_signal_set(PWR_EC_PCH_SYS_PWROK, 0);
}

static void x86_non_dsx_mtl_s0_entry(void *data)
{
	if (AP_PWRSEQ_DT_VALUE(sys_pwrok_delay)) {
		k_msleep(AP_PWRSEQ_DT_VALUE(sys_pwrok_delay));
	}
}

/* Generate SYS_PWROK->SOC if needed by system */
static void x86_non_dsx_mtl_s0_run(void *data)
{
	/* Loop through all PWROK signals defined by the board */
	if (power_signal_get(PWR_ALL_SYS_PWRGD) == 0) {
		LOG_DBG("PG_EC_ALL_SYS_PWRGD deasserted, "
			"shutting AP off!");
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S3);
		return;
	}

	power_signal_set(PWR_EC_PCH_SYS_PWROK, 1);
	/* PCH_PWROK is set to combined result of ALL_SYS_PWRGD and SLP_S3 */
	power_signal_set(PWR_PCH_PWROK,
			 !power_signal_get(PWR_SLP_S3));
}

AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_S0,
			      x86_non_dsx_mtl_s0_entry,
			      x86_non_dsx_mtl_s0_run,
			      x86_non_dsx_mtl_s0_exit)
