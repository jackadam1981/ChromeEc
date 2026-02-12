/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_pwrseq.h"
#include "fakes.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include <ap_power/ap_power.h>
#include <ap_power/ap_power_events.h>
#include <ap_power/ap_power_interface.h>
#include <ap_power_override_functions.h>
#include <mock/ap_power_events.h>
#include <mock/power_signals.h>
#include <power_signals.h>
#include <x86_power_signals.h>

#if defined(CONFIG_AP_PWRSEQ_DRIVER)
#include <ap_power/ap_pwrseq_sm.h>
#endif

#define X86_NON_DSX_MTL_FORCE_SHUTDOWN_TO_MS 50

static bool force_shutdown_pch_pwrok_off;
static bool force_shutdown_sys_pwrok_off;
static bool force_shutdown_rsmrst_on;
static bool force_shutdown_pp3300_off;
static bool force_shutdown_pp5000_off;

int mock_power_signal_set_ap_force_shutdown(enum power_signal signal, int value)
{
	if (IS_ENABLED(CONFIG_TEST_AP_PWRSEQ_FORCE_SHUTDOWN_PWROK) &&
	    (!force_shutdown_pch_pwrok_off || !force_shutdown_sys_pwrok_off)) {
		switch (signal) {
		case PWR_PCH_PWROK:
			zassert_equal(value, 0, "Deassert PWR_PCH_PWROK");
			force_shutdown_pch_pwrok_off = true;
			return 0;
		case PWR_EC_PCH_SYS_PWROK:
			zassert_equal(value, 0,
				      "Deassert PWR_EC_PCH_SYS_PWROK");
			force_shutdown_sys_pwrok_off = true;
			return 0;
		default:
			zassert_unreachable(
				"Deassert PCH_PWROK, SYS_PWROK first. Got "
				"signal: %d, value: %d",
				signal, value);
			return -1;
		}
	}

	if (!force_shutdown_rsmrst_on) {
		zassert_true(signal == PWR_EC_PCH_RSMRST && value == 1,
			     "Expected PCH_RSMRST. "
			     "Got signal: %d, value: %d",
			     signal, value);
		force_shutdown_rsmrst_on = true;
		return 0;
	}

	if (!force_shutdown_pp3300_off) {
		zassert_true(signal == PWR_EN_PP3300_A && value == 0,
			     "Expected PP3300_A. "
			     "Got  signal: %d, value: %d",
			     signal, value);
		force_shutdown_pp3300_off = true;
		return 0;
	}

#if CONFIG_TEST_AP_PWRSEQ_PP5500
	zassert_true(signal == PWR_EN_PP5000_A && value == 0,
		     "Expected PP5000_A. "
		     "Got signal: %d, value: %d",
		     signal, value);
	force_shutdown_pp5000_off = true;
	return 0;
#endif /* CONFIG_TEST_AP_PWRSEQ_PP5500 */

	zassert_unreachable(
		"Wrong input received. power_signal_set_fake.call_count: %d, "
		"signal: %d, value: %d",
		power_signal_set_fake.call_count, signal, value);
	return -1;
}

int mock_power_signal_set_ap_power_action_g3_s5(enum power_signal signal,
						int value)
{
	if ((signal == PWR_EN_PP3300_A) && (value == 1)) {
		return 0;
	}

	zassert_unreachable("Wrong input received. signal: %d, value: %d",
			    signal, value);
	return -1;
}

int mock_power_signal_get_ap_force_shutdown_retries(enum power_signal signal)
{
	if (signal == PWR_RSMRST_PWRGD) {
		return 1;
	}

	zassert_unreachable("Wrong input received. signal: %d", signal);
	return -1;
}

int mock_power_signal_get_ap_force_shutdown(enum power_signal signal)
{
	if (signal == PWR_RSMRST_PWRGD) {
		if (power_signal_get_fake.call_count <= 5) {
			return 1;
		}
		return 0;
	}
	zassert_unreachable("Wrong input received. signal: %d", signal);
	return -1;
}

int mock_power_signal_get_check_power_rails_enabled_0(enum power_signal signal)
{
	if (signal == PWR_EN_PP3300_A) {
		return 0;
	}
	zassert_unreachable("Wrong input received. signal: %d", signal);
	return -1;
}

int mock_power_signal_get_check_power_rails_enabled_1(enum power_signal signal)
{
	if (signal == PWR_EN_PP3300_A) {
		return 1;
	}
	zassert_unreachable("Wrong input received. signal: %d", signal);
	return -1;
}

int mock_power_wait_mask_signals_timeout_0(power_signal_mask_t want,
					   power_signal_mask_t mask,
					   int timeout)
{
	zassert_equal(want, IN_PGOOD_ALL_CORE);
	zassert_equal(mask, IN_PGOOD_ALL_CORE);
	zassert_equal(timeout, AP_PWRSEQ_DT_VALUE(wait_signal_timeout));
	return 0;
}

int mock_power_wait_mask_signals_timeout_1(power_signal_mask_t want,
					   power_signal_mask_t mask,
					   int timeout)
{
	zassert_equal(want, IN_PGOOD_ALL_CORE);
	zassert_equal(mask, IN_PGOOD_ALL_CORE);
	zassert_equal(timeout, AP_PWRSEQ_DT_VALUE(wait_signal_timeout));
	return 1;
}

void mock_ap_power_ev_send_callbacks(enum ap_power_events event)
{
	zassert_equal(event, AP_POWER_PRE_INIT);
}

#if defined(CONFIG_AP_PWRSEQ_DRIVER)
static int chipset_run_count;

int mock_power_signal_set_ap_power_action_g3_run_1(enum power_signal signal,
						   int value)
{
#if CONFIG_TEST_AP_PWRSEQ_PP5500
	if ((signal == PWR_EN_PP5000_A) && (value == 1)) {
		return 0;
	}
#endif

	if ((signal == PWR_EN_PP3300_A) && (value == 1)) {
		return 0;
	}

	zassert_unreachable("Wrong input received");
	return -1;
}

int mock_power_signal_get_ap_power_action_g3_run_1(enum power_signal signal)
{
	if (signal == PWR_EN_PP3300_A) {
		return 1;
	}
	zassert_unreachable("Wrong input received");
	return -1;
}

int mock_power_signal_get_ap_power_action_g3_run_0(enum power_signal signal)
{
	if (signal == PWR_EN_PP3300_A) {
		return 0;
	}
	zassert_unreachable("Wrong input received");
	return -1;
}

/**
 * This will help to verify that function `board_ap_power_action_g3_run` is
 * returning proper value by evaluating power signals.
 * This function will only be called when `board_ap_power_action_g3_run`
 * returns 0 (zero).
 **/
static int chipset_ap_power_action_g3_run(void *data)
{
	chipset_run_count++;

	return 0;
}

AP_POWER_CHIPSET_STATE_DEFINE(G3, NULL, chipset_ap_power_action_g3_run, NULL);

#if CONFIG_AP_PWRSEQ_S0IX
AP_POWER_CHIPSET_SUB_STATE_DEFINE(S0ix, NULL, x86_non_dsx_mtl_s0ix_run, NULL,
				  S0);
#endif /* CONFIG_AP_PWRSEQ_S0IX */
#endif

static void board_power_before(void *fixture)
{
	ARG_UNUSED(fixture);

	force_shutdown_pch_pwrok_off = false;
	force_shutdown_sys_pwrok_off = false;
	force_shutdown_rsmrst_on = false;
	force_shutdown_pp3300_off = false;
	force_shutdown_pp5000_off = false;

	RESET_FAKE(power_signal_set);
	RESET_FAKE(power_signal_get);
	RESET_FAKE(power_wait_mask_signals_timeout);
	RESET_FAKE(ap_power_ev_send_callbacks);
#if defined(CONFIG_AP_PWRSEQ_DRIVER)
	chipset_run_count = 0;
#endif
}

ZTEST_USER(board_power, test_board_ap_power_force_shutdown)
{
	power_signal_set_fake.custom_fake =
		mock_power_signal_set_ap_force_shutdown;
	power_signal_get_fake.custom_fake =
		mock_power_signal_get_ap_force_shutdown;

	board_ap_power_force_shutdown();

	if (IS_ENABLED(CONFIG_TEST_AP_PWRSEQ_PP5500)) {
		zassert_true(force_shutdown_pp5000_off,
			     "Expected to reach PP5000_A deassertion.");
	} else {
		zassert_true(force_shutdown_pp3300_off,
			     "Expected to reach PP3300_A deassertion.");
	}
	zassert_equal(7, power_signal_get_fake.call_count);
}

ZTEST_USER(board_power, test_board_ap_power_force_shutdown_timeout)
{
	power_signal_set_fake.custom_fake =
		mock_power_signal_set_ap_force_shutdown;
	power_signal_get_fake.custom_fake =
		mock_power_signal_get_ap_force_shutdown_retries;

	const uint32_t start_ms = k_uptime_get();

	board_ap_power_force_shutdown();

	const uint32_t end_ms = k_uptime_get();

	zassert_true((end_ms - start_ms) >=
		     X86_NON_DSX_MTL_FORCE_SHUTDOWN_TO_MS);

	if (IS_ENABLED(CONFIG_TEST_AP_PWRSEQ_PP5500)) {
		zassert_true(force_shutdown_pp5000_off,
			     "Expected to reach PP5000_A deassertion.");
	} else {
		zassert_true(force_shutdown_pp3300_off,
			     "Expected to reach PP3300_A deassertion.");
	}
	zassert_true(power_signal_get_fake.call_count > 2);
}

#if defined(CONFIG_AP_PWRSEQ_DRIVER)
ZTEST_USER(board_power, test_board_ap_power_action_g3_run_0)
{
	const struct device *dev = ap_pwrseq_get_instance();

	ap_pwrseq_start(dev, AP_POWER_STATE_G3);

	power_signal_get_fake.custom_fake =
		mock_power_signal_get_ap_power_action_g3_run_0;

	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_SIGNAL);
	/* Buffer time to process event */
	k_msleep(5);
	zassert_equal(0, chipset_run_count);
}

ZTEST_USER(board_power, test_board_ap_power_action_g3_run_1)
{
	const struct device *dev = ap_pwrseq_get_instance();

	power_signal_set_fake.custom_fake =
		mock_power_signal_set_ap_power_action_g3_run_1;
	power_signal_get_fake.custom_fake =
		mock_power_signal_get_ap_power_action_g3_run_0;

	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_STARTUP);
	/* Buffer time to process event */
	k_msleep(5);
	zassert_equal(0, chipset_run_count);
}

ZTEST_USER(board_power, test_board_ap_power_action_g3_run_2)
{
	const struct device *dev = ap_pwrseq_get_instance();

	power_signal_set_fake.custom_fake =
		mock_power_signal_set_ap_power_action_g3_run_1;
	power_signal_get_fake.custom_fake =
		mock_power_signal_get_ap_power_action_g3_run_1;

	ap_pwrseq_post_event(dev, AP_PWRSEQ_EVENT_POWER_STARTUP);
	/* Buffer time to process event */
	k_msleep(5);
	zassert_equal(1, chipset_run_count);
}
#else
ZTEST_USER(board_power, test_board_ap_power_check_power_rails_enabled_0)
{
	power_signal_get_fake.custom_fake =
		mock_power_signal_get_check_power_rails_enabled_0;

	zassert_equal(0, board_ap_power_check_power_rails_enabled());
	zassert_equal(1, power_signal_get_fake.call_count);
}

ZTEST_USER(board_power, test_board_ap_power_check_power_rails_enabled_1)
{
	power_signal_get_fake.custom_fake =
		mock_power_signal_get_check_power_rails_enabled_1;

	zassert_equal(1, board_ap_power_check_power_rails_enabled());
	zassert_equal(1, power_signal_get_fake.call_count);
}

ZTEST_USER(board_power, test_board_ap_power_action_g3_s5_0)
{
	power_signal_set_fake.custom_fake =
		mock_power_signal_set_ap_power_action_g3_s5;
	power_wait_mask_signals_timeout_fake.custom_fake =
		mock_power_wait_mask_signals_timeout_0;
	ap_power_ev_send_callbacks_fake.custom_fake =
		mock_ap_power_ev_send_callbacks;

	board_ap_power_action_g3_s5();

	zassert_equal(1, power_signal_set_fake.call_count);
	zassert_equal(1, power_wait_mask_signals_timeout_fake.call_count);
	zassert_equal(1, ap_power_ev_send_callbacks_fake.call_count);
}

ZTEST_USER(board_power, test_board_ap_power_action_g3_s5_1)
{
	power_signal_set_fake.custom_fake =
		mock_power_signal_set_ap_power_action_g3_s5;
	power_wait_mask_signals_timeout_fake.custom_fake =
		mock_power_wait_mask_signals_timeout_1;
	ap_power_ev_send_callbacks_fake.custom_fake =
		mock_ap_power_ev_send_callbacks;

	board_ap_power_action_g3_s5();

	zassert_equal(1, power_signal_set_fake.call_count);
	zassert_equal(1, power_wait_mask_signals_timeout_fake.call_count);
	zassert_equal(0, ap_power_ev_send_callbacks_fake.call_count);
}
#endif /* CONFIG_AP_PWRSEQ_DRIVER */

ZTEST_SUITE(board_power, NULL, NULL, board_power_before, NULL, NULL);
