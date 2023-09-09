/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Unit tests for program/nissa/src/board_power.c.
 */
#include "emul/emul_power_signals.h"

#include <zephyr/drivers/espi.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/include/emul/emul_power_signals.h>
#include <zephyr/ztest.h>

#include <ap_power/ap_pwrseq_sm.h>
#include <ap_power_override_functions.h>
#include <common.h>
#include <power_signals.h>

int chipset_g3_run_count;
int chipset_s0_run_count;

static void setup_test(void *fixture)
{
	power_signal_init();
}

static void after_test(void *fixture)
{
	power_signal_emul_unload();
}

ZTEST_SUITE(nissa_board_power, NULL, setup_test, NULL, after_test, NULL);

ZTEST(nissa_board_power, test_board_ap_power_g3_run_0)
{
	const struct device *dev = ap_pwrseq_get_instance();

	zassert_equal(0,
		      power_signal_emul_load(EMUL_POWER_SIGNAL_TEST_PLATFORM(
			      tp_power_down_ok)),
		      "Unable to load test platform `tp_power_down_ok`");
	ap_pwrseq_start(dev, AP_POWER_STATE_G3);
	zassert_equal(1, power_signal_get(PWR_SLP_SUS));
	zassert_equal(0, power_signal_get(PWR_RSMRST));
	zassert_equal(0, power_signal_get(PWR_EN_PP3300_A));
	zassert_equal(0, power_signal_get(PWR_EC_SOC_DSW_PWROK));
	zassert_equal(0, power_signal_get(PWR_EC_PCH_RSMRST));
}

ZTEST(nissa_board_power, test_board_ap_power_g3_run_1)
{
	const struct device *dev = ap_pwrseq_get_instance();

	zassert_equal(0,
		      power_signal_emul_load(
			      EMUL_POWER_SIGNAL_TEST_PLATFORM(tp_power_up_ok)),
		      "Unable to load test platform `tp_power_up_ok`");

	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_G3);
	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_SIGNAL);
	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_G3);
}

ZTEST(nissa_board_power, test_board_ap_power_g3_run_2)
{
	const struct device *dev = ap_pwrseq_get_instance();

	zassert_equal(0,
		      power_signal_emul_load(
			      EMUL_POWER_SIGNAL_TEST_PLATFORM(tp_power_up_ok)),
		      "Unable to load test platform `tp_power_up_ok`");

	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_G3);
	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_STARTUP);
	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_S0);
}

static int chipset_ap_power_g3_run(void *data)
{
	return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S0);
}

AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_G3, NULL, chipset_ap_power_g3_run,
			      NULL);

static int x86_non_dsx_adlp_s0ix_run(void *data)
{
	return 0;
}

AP_POWER_CHIPSET_SUB_STATE_DEFINE(AP_POWER_STATE_S0IX, NULL,
				  x86_non_dsx_adlp_s0ix_run, NULL,
				  AP_POWER_STATE_S0);
