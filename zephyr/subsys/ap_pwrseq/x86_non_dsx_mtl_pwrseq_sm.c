/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <x86_non_dsx_common_pwrseq_sm_handler.h>

#include "ap_power/ap_pwrseq_sm.h"

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

static int x86_non_dsx_mtl_g3_run(void *data)
{
	/*
	 * Power rail must be enabled by application, now check if chipset is
	 * ready.
	 */
	if (power_wait_signals_timeout(POWER_SIGNAL_MASK(PWR_RSMRST),
		    AP_PWRSEQ_DT_VALUE(wait_signal_timeout))) {
		return 1;
	}

	return 0;
}

AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_G3,
			      NULL,
			      x86_non_dsx_mtl_g3_run,
			      NULL)

static int x86_non_dsx_mtl_s3_entry(void *data)
{
	power_signal_set(PWR_PCH_PWROK, 0);
	power_signal_set(PWR_EC_PCH_SYS_PWROK, 0);

	return 0;
}

static int x86_non_dsx_mtl_s3_run(void *data)
{
	if (power_signal_get(PWR_RSMRST) == 0) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	}

	if (power_signal_get(PWR_SLP_S4)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S4);
	}

	if (power_signal_get(PWR_EC_PCH_SYS_PWROK) &&
	    power_signal_get(PWR_PCH_PWROK) &&
	    power_signal_get(PWR_ALL_SYS_PWRGD) &&
	    power_signal_get(PWR_EC_PCH_SYS_PWROK)) {
		return 0;
	}

	return 1;
}

AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_S3,
			      x86_non_dsx_mtl_s3_entry,
			      x86_non_dsx_mtl_s3_run,
			      NULL)

static int x86_non_dsx_mtl_s0_run(void *data)
{
	if (power_signal_get(PWR_RSMRST) == 0) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	}

	return 0;
}

AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_S0,
			      NULL,
			      x86_non_dsx_mtl_s0_run,
			      NULL)
