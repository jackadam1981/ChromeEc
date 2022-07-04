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

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

#define X86_NON_DSX_ADLP_NONPWRSEQ_FORCE_SHUTDOWN_TO_MS 5

static bool s0_stable;

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

	if (s0_stable) {
		/* Enable these power signals in case of sudden shutdown */
		power_signal_enable(PWR_DSW_PWROK);
		power_signal_enable(PWR_PG_PP1P05);
	}

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
	s0_stable = false;
}

void board_ap_power_action_g3_s5(void)
{
	power_signal_enable(PWR_DSW_PWROK);
	power_signal_enable(PWR_PG_PP1P05);

	LOG_DBG("Turning on PWR_EN_PP5000_A and PWR_EN_PP3300_A");
	power_signal_set(PWR_EN_PP5000_A, 1);
	power_signal_set(PWR_EN_PP3300_A, 1);

	power_wait_signals_timeout(IN_PGOOD_ALL_CORE,
				   AP_PWRSEQ_DT_VALUE(wait_signal_timeout));

	generate_ec_soc_dsw_pwrok_handler(AP_PWRSEQ_DT_VALUE(dsw_pwrok_delay));
	s0_stable = false;
}

void board_ap_power_action_s3_s0(void)
{
	s0_stable = false;
}

void board_ap_power_action_s0_s3(void)
{
	power_signal_enable(PWR_DSW_PWROK);
	power_signal_enable(PWR_PG_PP1P05);
	s0_stable = false;
}

void board_ap_power_action_s0(void)
{
	if (s0_stable) {
		return;
	}
	LOG_INF("Reaching S0");
	power_signal_disable(PWR_DSW_PWROK);
	power_signal_disable(PWR_PG_PP1P05);
	s0_stable = true;
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

bool board_ap_power_check_power_rails_enabled(void)
{
	return power_signal_get(PWR_EN_PP3300_A) &&
	       power_signal_get(PWR_EN_PP5000_A) &&
	       power_signal_get(PWR_EC_SOC_DSW_PWROK);
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

#ifdef CONFIG_SOC_IT8XXX2
/*
 * When eSPI CS# is held low, it prevents IT8xxx2 from entering deep doze.
 * To allow deep doze and save power, disable the eSPI inputs while the AP is
 * in G3.
 */
#include <soc_espi.h>

static const struct device *const ESPI_DEVICE =
	DEVICE_DT_GET(DT_NODELABEL(espi0));

static void espi_enable_callback(struct ap_power_ev_callback *cb,
				 struct ap_power_ev_data data)
{
	bool enable = data.event == AP_POWER_PRE_INIT;

	LOG_DBG("%sabling eSPI in response to AP power event",
		enable ? "en" : "dis");
	espi_it8xxx2_enable_pad_ctrl(ESPI_DEVICE,
				     data.event == AP_POWER_PRE_INIT);
}

static int init_espi_enable_callback(const struct device *unused)
{
	static struct ap_power_ev_callback cb;
	int key;

	ap_power_ev_init_callback(&cb, espi_enable_callback,
				  AP_POWER_PRE_INIT | AP_POWER_HARD_OFF);
	ap_power_ev_add_callback(&cb);

	/*
	 * Critical section: AP power must not change state between read and
	 * update, otherwise we might be leaving it disabled if it leaves G3
	 * after we read state but before we disable eSPI.
	 */
	key = irq_lock();
	if (ap_power_in_state(AP_POWER_STATE_HARD_OFF)) {
		LOG_DBG("AP off; disabling eSPI for now");
		espi_it8xxx2_enable_pad_ctrl(ESPI_DEVICE, false);
	}
	irq_unlock(key);

	return 0;
}
SYS_INIT(init_espi_enable_callback, APPLICATION, 10);
#endif
