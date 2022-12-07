/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "ap_power/ap_pwrseq_sm.h"
#include <ap_power/ap_power_interface.h>
#include <ap_power_override_functions.h>
#include <power_signals.h>
#include <timer.h>
#include <x86_power_signals.h>

static bool signal_PWR_ALL_SYS_PWRGD;
static bool signal_PWR_DSW_PWROK;
static bool signal_PWR_PG_PP1P05;

int board_power_signal_set(enum power_signal signal, int value)
{
	switch (signal) {
	default:
		zassert_unreachable("Unknown signal");
		return -1;

	case PWR_ALL_SYS_PWRGD:
		signal_PWR_ALL_SYS_PWRGD = value;
		return 0;

	case PWR_DSW_PWROK:
		signal_PWR_DSW_PWROK = value;
		return 0;

	case PWR_PG_PP1P05:
		signal_PWR_PG_PP1P05 = value;
		return 0;
	}
}

int board_power_signal_get(enum power_signal signal)
{
	switch (signal) {
	default:
		zassert_unreachable("Unknown signal");
		return -1;

	case PWR_ALL_SYS_PWRGD:
		return signal_PWR_ALL_SYS_PWRGD;

	case PWR_DSW_PWROK:
		return signal_PWR_DSW_PWROK;

	case PWR_PG_PP1P05:
		return signal_PWR_PG_PP1P05;
	}
}

void board_ap_power_force_shutdown(void)
{
}

static void generate_ec_soc_dsw_pwrok_handler(void)
{
	int in_sig_val = power_signal_get(PWR_DSW_PWROK);

	if (in_sig_val != power_signal_get(PWR_EC_SOC_DSW_PWROK)) {
		power_signal_set(PWR_EC_SOC_DSW_PWROK, 1);
	}
}

static int board_ap_power_g3_run(void *data)
{
	if (IS_EVENT_SET(data, AP_PWRSEQ_EVENT_POWER_STARTUP)) {
		power_signal_enable(PWR_DSW_PWROK);

		power_signal_set(PWR_EN_PP3300_A, 1);
		power_signal_set(PWR_EN_PP5000_A, 1);

		power_wait_signals_timeout(POWER_SIGNAL_MASK(PWR_DSW_PWROK),
				   AP_PWRSEQ_DT_VALUE(wait_signal_timeout));
	}

	generate_ec_soc_dsw_pwrok_handler();

	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_G3,
			  NULL,
			  board_ap_power_g3_run,
			  NULL)

static int board_ap_power_s3_run(void *data)
{
	static int all_sys_pwrgd_retry;

	power_wait_signals_timeout(POWER_SIGNAL_MASK(PWR_ALL_SYS_PWRGD),
				   AP_PWRSEQ_DT_VALUE(all_sys_pwrgd_timeout));

	if (power_signal_get(PWR_ALL_SYS_PWRGD) == 0) {
		if (all_sys_pwrgd_retry++ >= 2) {
			all_sys_pwrgd_retry = 0;
			return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
		}
		return 1;
	}

	all_sys_pwrgd_retry = 0;

	if (power_signal_get(PWR_PCH_PWROK) == 0) {
		k_msleep(AP_PWRSEQ_DT_VALUE(pch_pwrok_delay));
		power_signal_set(PWR_PCH_PWROK, 1);
	}

	if (power_signal_get(PWR_VCCST_PWRGD) == 0) {
		k_msleep(AP_PWRSEQ_DT_VALUE(vccst_pwrgd_delay));
		power_signal_set(PWR_VCCST_PWRGD, 1);
	}

	if (power_signal_get(PWR_EC_PCH_SYS_PWROK) == 0) {
		k_msleep(AP_PWRSEQ_DT_VALUE(sys_pwrok_delay));
		/* Check if we lost power while waiting. */
		power_signal_set(PWR_EC_PCH_SYS_PWROK, 1);
	}

	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_S3,
			  NULL,
			  board_ap_power_s3_run,
			  NULL)

static int board_ap_power_s0_run(void *data)
{
	if (power_signal_get(PWR_PCH_PWROK) == 0) {
		k_msleep(AP_PWRSEQ_DT_VALUE(pch_pwrok_delay));
		power_signal_set(PWR_PCH_PWROK, 1);
	}

	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_S0,
			  NULL,
			  board_ap_power_s0_run,
			  NULL)
