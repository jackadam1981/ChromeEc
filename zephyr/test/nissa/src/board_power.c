/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Unit tests for program/nissa/src/board_power.c.
 */
#include "ap_power/ap_power.h"
#include "emul/emul_power_signals.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

#include <ap_power/ap_pwrseq_sm.h>
#include <ap_power_override_functions.h>
#include <common.h>
#include <power_signals.h>

int chipset_s0_run_count;

static void setup_test(void *fixture)
{
	power_signal_init();
}

static void after_test(void *fixture)
{
	power_signal_emul_unload();
	chipset_s0_run_count = 0;
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
		      power_signal_emul_load(EMUL_POWER_SIGNAL_TEST_PLATFORM(
			      tp_power_up_fail)),
		      "Unable to load test platform `tp_power_up_fail`");

	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_G3);
	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_STARTUP);
	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_G3);
}

ZTEST(nissa_board_power, test_board_ap_power_g3_run_3)
{
	const struct device *dev = ap_pwrseq_get_instance();

	zassert_equal(0,
		      power_signal_emul_load(
			      EMUL_POWER_SIGNAL_TEST_PLATFORM(tp_power_up_ok)),
		      "Unable to load test platform `tp_power_up_ok`");

	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_G3);
	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_STARTUP);
	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_S3);
}

ZTEST(nissa_board_power, test_board_ap_power_s0_run_0)
{
	const struct device *dev = ap_pwrseq_get_instance();
	const struct gpio_dt_spec *gpio_all_sys_pwrgd_spec =
		GPIO_DT_FROM_NODELABEL(gpio_all_sys_pwrgd);

	zassert_equal(
		0,
		power_signal_emul_load(EMUL_POWER_SIGNAL_TEST_PLATFORM(tp_s0)),
		"Unable to load test platform `tp_s0`");

	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_S3);

	gpio_emul_input_set(gpio_all_sys_pwrgd_spec->port,
			    gpio_all_sys_pwrgd_spec->pin, 0);
	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_SIGNAL);
	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_S0);
	zassert_equal(chipset_s0_run_count, 0);

	/* This will assert `PWR_PG_PP1P0` */
	power_signal_set(PWR_EN_PP3300_A, 1);
	k_sleep(K_MSEC(50));
	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_SIGNAL);
	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_S0);
	zassert_equal(chipset_s0_run_count, 0);

	gpio_emul_input_set(gpio_all_sys_pwrgd_spec->port,
			    gpio_all_sys_pwrgd_spec->pin, 1);
	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_SIGNAL);
	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_S0);
	zassert_equal(chipset_s0_run_count, 1);
}

ZTEST(nissa_board_power, test_board_ap_power_s0_run_1)
{
	const struct device *dev = ap_pwrseq_get_instance();
	const struct gpio_dt_spec *gpio_all_sys_pwrgd_spec =
		GPIO_DT_FROM_NODELABEL(gpio_all_sys_pwrgd);

	zassert_equal(0,
		      power_signal_emul_load(
			      EMUL_POWER_SIGNAL_TEST_PLATFORM(tp_s0_to_s3)),
		      "Unable to load test platform `tp_s0_to_s3`");

	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_S0);
	gpio_emul_input_set(gpio_all_sys_pwrgd_spec->port,
			    gpio_all_sys_pwrgd_spec->pin, 1);
	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_SIGNAL);
	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_S0);
	zassert_equal(chipset_s0_run_count, 1);

	power_signal_set(PWR_EN_PP3300_A, 0);
	k_sleep(K_MSEC(50));
	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_SHUTDOWN);
	zassert_equal(ap_pwrseq_get_current_state(dev), AP_POWER_STATE_S3);
	zassert_equal(chipset_s0_run_count, 1);
}

/**
 * Supporting functions for test
 **/
static int chipset_ap_power_s0_run(void *data)
{
	chipset_s0_run_count++;
	return 0;
}

AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_S0, NULL, chipset_ap_power_s0_run,
			      NULL);

static int chipset_ap_power_s3_run(void *data)
{
	if (ap_pwrseq_sm_is_event_set(data, AP_PWRSEQ_EVENT_POWER_SIGNAL)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S0);
	}

	return 0;
}

AP_POWER_CHIPSET_STATE_DEFINE(AP_POWER_STATE_S3, NULL, chipset_ap_power_s3_run,
			      NULL);

static int chipset_ap_power_g3_run(void *data)
{
	return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S3);
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
