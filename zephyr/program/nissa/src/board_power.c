/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "ap_power/ap_pwrseq_sm.h"
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <ap_power/ap_power_interface.h>
#include <ap_power_override_functions.h>
#include <power_signals.h>
#include <x86_power_signals.h>
#include "gpio_signal.h"
#include "gpio/gpio.h"

LOG_MODULE_DECLARE(ap_pwrseq, LOG_LEVEL_INF);

#define X86_NON_DSX_ADLP_NONPWRSEQ_FORCE_SHUTDOWN_TO_MS 5

static void generate_ec_soc_dsw_pwrok_handler(int delay)
{
	int in_sig_val = power_signal_get(PWR_DSW_PWROK);

	if (in_sig_val != power_signal_get(PWR_EC_SOC_DSW_PWROK)) {
		if (in_sig_val)
			k_msleep(delay);
		power_signal_set(PWR_EC_SOC_DSW_PWROK, in_sig_val);
	}
}

void board_ap_power_force_shutdown(void)
{
	/* Bringing these signals down will move power sequence state machine */
	power_signal_set(PWR_EC_SOC_DSW_PWROK, 0);
	power_signal_set(PWR_EC_PCH_RSMRST, 0);
}

int board_power_signal_get(enum power_signal signal)
{
	switch (signal) {
	default:
		LOG_ERR("Unknown signal for board get: %d", signal);
		return -EINVAL;
	case PWR_ALL_SYS_PWRGD:
		/*
		 * All system power is good.
		 * Checks that PWR_SLP_S3 is off, and
		 * the GPIO signal for all power good is set,
		 * and that the 1.05 volt line is ready.
		 */
		if (power_signal_get(PWR_SLP_S3)) {
			return 0;
		}
		if (!gpio_pin_get_dt(
			    GPIO_DT_FROM_NODELABEL(gpio_all_sys_pwrgd))) {
			return 0;
		}
		if (!power_signal_get(PWR_PG_PP1P05)) {
			return 0;
		}
		return 1;
	}
}

int board_power_signal_set(enum power_signal signal, int value)
{
	return -EINVAL;
}

static int board_ap_power_g3_entry(void *data)
{
	power_signal_set(PWR_VCCST_PWRGD, 0);
	power_signal_set(PWR_PCH_PWROK, 0);
	power_signal_set(PWR_EC_PCH_SYS_PWROK, 0);

	power_signal_set(PWR_EN_PP5000_A, 0);
	power_signal_set(PWR_EN_PP3300_A, 0);

	return 0;
}

static int board_ap_power_g3_run(void *data)
{
	if (IS_EVENT_SET(data, AP_PWRSEQ_EVENT_POWER_STARTUP)) {
		power_signal_enable(PWR_DSW_PWROK);
		power_signal_enable(PWR_PG_PP1P05);

		LOG_INF("Turning on PWR_EN_PP5000_A and PWR_EN_PP3300_A");

		power_signal_set(PWR_EN_PP5000_A, 1);
		power_signal_set(PWR_EN_PP3300_A, 1);

		power_wait_signals_timeout(POWER_SIGNAL_MASK(PWR_DSW_PWROK),
				   AP_PWRSEQ_DT_VALUE(wait_signal_timeout));
	}

	generate_ec_soc_dsw_pwrok_handler(AP_PWRSEQ_DT_VALUE(dsw_pwrok_delay));

	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_G3,
		      board_ap_power_g3_entry,
		      board_ap_power_g3_run,
		      NULL)

static int board_ap_power_s5_run(void *data)
{
	if (power_signal_get(PWR_EN_PP3300_A) &&
	    power_signal_get(PWR_EN_PP5000_A) &&
	    power_signal_get(PWR_EC_SOC_DSW_PWROK)) {
		return 0;
	}

	LOG_ERR("Power rails are not ready");

	return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_S5,
		      NULL,
		      board_ap_power_s5_run,
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

	if (!power_signals_on(POWER_SIGNAL_MASK(PWR_VCCST_PWRGD))) {
		k_msleep(AP_PWRSEQ_DT_VALUE(vccst_pwrgd_delay));
		power_signal_set(PWR_VCCST_PWRGD, 1);
	}

	/* Pass though PCH_PWROK */
	if (power_signal_get(PWR_PCH_PWROK) == 0) {
		k_msleep(AP_PWRSEQ_DT_VALUE(pch_pwrok_delay));
		power_signal_set(PWR_PCH_PWROK, 1);
	}

	if (power_signal_get(PWR_EC_PCH_SYS_PWROK) == 0) {
		k_msleep(AP_PWRSEQ_DT_VALUE(sys_pwrok_delay));
		/* Check if we lost power while waiting. */
		if (power_signal_get(PWR_ALL_SYS_PWRGD) == 0) {
			LOG_DBG("PG_EC_ALL_SYS_PWRGD deasserted, "
				"shutting AP off!");
			return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
		}
		LOG_INF("Turning on PWR_EC_PCH_SYS_PWROK");
		power_signal_set(PWR_EC_PCH_SYS_PWROK, 1);
		/* PCH will now release PLT_RST */
	}

	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_S3,
		      NULL,
		      board_ap_power_s3_run,
		      NULL)

static int board_ap_power_s0_entry(void *data)
{
	power_signal_disable(PWR_DSW_PWROK);
	power_signal_disable(PWR_PG_PP1P05);

	return 0;
}

static int board_ap_power_s0_exit(void *arg)
{
	power_signal_enable(PWR_DSW_PWROK);
	power_signal_enable(PWR_PG_PP1P05);

	return 1;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_S0,
		      board_ap_power_s0_entry,
		      NULL,
		      board_ap_power_s0_exit)
