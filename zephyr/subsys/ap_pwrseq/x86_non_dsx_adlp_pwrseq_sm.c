/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <x86_non_dsx_common_pwrseq_sm_handler.h>
#include <ap_power/ap_pwrseq_sm.h>

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

static int check_pch_out_of_suspend(void)
{
	int ret;
	/*
	 * Wait for SLP_SUS deasserted.
	 */
	ret = power_wait_signals_timeout(POWER_SIGNAL_MASK(PWR_SLP_SUS),
					      IN_PCH_SLP_SUS_WAIT_TIME_MS);
	if (ret == 0) {
		LOG_DBG("SLP_SUS now %d", power_signal_get(PWR_SLP_SUS));
		return 1;
	}
	LOG_ERR("wait SLP_SUS deassertion timeout");
	return 0; /* timeout */
}

/* Chipset specific power state machine handler */
static int x86_non_dsx_adlp_g3_run(void *data)
{
	/*
	 * Now wait for SLP_SUS_L to go high based on tPCH32. If this
	 * signal doesn't go high within 250 msec then go back to G3.
	 */
	if (check_pch_out_of_suspend()) {
		return 0;
	}

	return 1;
}

AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_G3,
			      NULL,
			      x86_non_dsx_adlp_g3_run,
			      NULL)


static int x86_non_dsx_adlp_s4_run(void *data)
{
	if (power_signal_get(PWR_SLP_SUS)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	}

	return 0;
}

AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_S4,
			      NULL,
			      x86_non_dsx_adlp_s4_run,
			      NULL)

static int x86_non_dsx_adlp_s3_run(void *data)
{
	if (power_signal_get(PWR_SLP_SUS)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	}

	if (power_signal_get(PWR_SLP_S4)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S4);
	}

	if (power_signal_get(PWR_VCCST_PWRGD) &&
	    power_signal_get(PWR_PCH_PWROK) &&
	    power_signal_get(PWR_EC_PCH_SYS_PWROK)) {
		/* These signals are set, moving forward */
		return 0;
	}

	return 1;
}

AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_S3,
			      NULL,
			      x86_non_dsx_adlp_s3_run,
			      NULL)

static int x86_non_dsx_adlp_s0_run(void *data)
{
	if (power_signal_get(PWR_SLP_SUS)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	}

	return 0;
}

AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_S0,
			      NULL,
			      x86_non_dsx_adlp_s0_run,
			      NULL)
