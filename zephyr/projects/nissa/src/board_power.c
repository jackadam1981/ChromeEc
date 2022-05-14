/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include <ap_power/ap_power.h>
#include <ap_power/ap_power_events.h>
#include <ap_power/ap_power_interface.h>
#include <ap_power_override_functions.h>
#include <power_signals.h>
#include <x86_power_signals.h>

#include "gpio_signal.h"
#include "gpio/gpio.h"

#include <ap_power/ap_pwrseq_sm.h>

LOG_MODULE_DECLARE(ap_pwrseq, LOG_LEVEL_INF);

#define  X86_NON_DSX_ADLP_NONPWRSEQ_FORCE_SHUTDOWN_TO_MS	5

static void generate_ec_soc_dsw_pwrok_handler(int delay)
{
	int in_sig_val = power_signal_get(PWR_DSW_PWROK);

	if (in_sig_val != power_signal_get(PWR_EC_SOC_DSW_PWROK)) {
		if (in_sig_val)
			k_msleep(delay);
		power_signal_set(PWR_EC_SOC_DSW_PWROK, 1);
	}
}

void board_ap_power_force_shutdown(void)
{
	int timeout_ms = X86_NON_DSX_ADLP_NONPWRSEQ_FORCE_SHUTDOWN_TO_MS;

	/* Enable these power signals in case of sudden shutdown */
	power_signal_enable(PWR_DSW_PWROK);
	power_signal_enable(PWR_PG_PP1P05);

	power_signal_set(PWR_EC_PCH_RSMRST, 0);
	power_signal_set(PWR_EC_SOC_DSW_PWROK, 0);

	while (power_signal_get(PWR_RSMRST) == 0 &&
	      power_signal_get(PWR_SLP_SUS) == 0 && timeout_ms > 0) {
		k_msleep(1);
		timeout_ms--;
	}
	if (power_signal_get(PWR_SLP_SUS) == 0) {
		LOG_WRN("SLP_SUS is not deasserted! Assuming G3");
	}

	if (power_signal_get(PWR_RSMRST) == 1) {
		LOG_WRN("RSMRST is not deasserted! Assuming G3");
	}

	power_signal_set(PWR_EN_PP3300_A, 0);

	power_signal_set(PWR_EN_PP5000_A, 0);

	timeout_ms = X86_NON_DSX_ADLP_NONPWRSEQ_FORCE_SHUTDOWN_TO_MS;
	while (power_signal_get(PWR_DSW_PWROK) && timeout_ms > 0) {
		k_msleep(1);
		timeout_ms--;
	};

	if (power_signal_get(PWR_DSW_PWROK))
		LOG_WRN("DSW_PWROK didn't go low!  Assuming G3.");

	power_signal_disable(PWR_DSW_PWROK);
	power_signal_disable(PWR_PG_PP1P05);
}

int board_ap_power_assert_pch_power_ok(void)
{
	/* Pass though PCH_PWROK */
	if (power_signal_get(PWR_PCH_PWROK) == 0) {
		k_msleep(AP_PWRSEQ_DT_VALUE(pch_pwrok_delay));
		power_signal_set(PWR_PCH_PWROK, 1);
	}

	return 0;
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

static void board_ap_power_g3_run(void *arg)
{
	struct ap_pwrseq_sm_data *data = arg;

	if (IS_EVENT_SET(data, AP_PWRSEQ_EVENT_POWER_BUTTON)) {
		power_signal_enable(PWR_DSW_PWROK);
		power_signal_enable(PWR_PG_PP1P05);

		LOG_DBG("Turning on PWR_EN_PP5000_A and PWR_EN_PP3300_A");
		power_signal_set(PWR_EN_PP5000_A, 1);
		power_signal_set(PWR_EN_PP3300_A, 1);
	}

	if(power_wait_signals_timeout(IN_PGOOD_ALL_CORE,
	   AP_PWRSEQ_DT_VALUE(wait_signal_timeout))){
		power_signal_set(PWR_EN_PP5000_A, 0);
		power_signal_set(PWR_EN_PP3300_A, 0);

		power_signal_disable(PWR_DSW_PWROK);
		power_signal_disable(PWR_PG_PP1P05);

		SET_NOT_READY(data);

		return;
	}

	generate_ec_soc_dsw_pwrok_handler(
		AP_PWRSEQ_DT_VALUE(dsw_pwrok_delay));
}

AP_POWER_STATE_DEFINE(AP_POWER_STATE_G3,
		      NULL,
		      board_ap_power_g3_run,
		      NULL)

static void board_ap_power_s5_run(void *arg)
{
	struct ap_pwrseq_sm_data *data = arg;

	if (power_signal_get(PWR_EN_PP3300_A) &&
	    power_signal_get(PWR_EN_PP5000_A) &&
	    power_signal_get(PWR_EC_SOC_DSW_PWROK)) {
		return;
	}
	/* power rails are not ready */
	SET_NOT_READY(data);
}

AP_POWER_STATE_DEFINE(AP_POWER_STATE_S5,
		      NULL,
		      board_ap_power_s5_run,
		      NULL)

static void board_ap_power_s0_entry(void *arg)
{
	power_signal_disable(PWR_DSW_PWROK);
	power_signal_disable(PWR_PG_PP1P05);
}

static void board_ap_power_s0_exit(void *arg)
{
	power_signal_enable(PWR_DSW_PWROK);
	power_signal_enable(PWR_PG_PP1P05);
}

AP_POWER_STATE_DEFINE(AP_POWER_STATE_S0,
		      board_ap_power_s0_entry,
		      NULL,
		      board_ap_power_s0_exit)
