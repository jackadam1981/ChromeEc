/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Include the board power header file or relevant declarations */
#include "ap_power_override_functions.h"
#include "gpio_signal.h"
#include "system_boot_time.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include <mock/ap_power_events.h>
#include <mock/power_signals.h>
#include <power_signals.h>

DEFINE_FAKE_VALUE_FUNC(int, power_signal_set, enum power_signal, int);
DEFINE_FAKE_VALUE_FUNC(int, power_signal_get, enum power_signal);
DEFINE_FAKE_VOID_FUNC(ap_power_ev_send_callbacks, enum ap_power_events);
DEFINE_FAKE_VALUE_FUNC(int, power_wait_mask_signals_timeout,
		       power_signal_mask_t, power_signal_mask_t, int);

#define X86_NON_DSX_FORCE_SHUTDOWN_TO_MS 50

/* Helper function to reset all fakes */
static void reset_test_fakes(void)
{
	RESET_FAKE(power_signal_set);
	RESET_FAKE(power_signal_get);
	RESET_FAKE(power_wait_mask_signals_timeout);
	RESET_FAKE(ap_power_ev_send_callbacks);
}

/* Define your test suite setup and teardown routines if needed */
static void test_suite_setup(void *state)
{
	/* Run before each test in the suite */
	reset_test_fakes();
}

ZTEST_SUITE(ptl_board_power, NULL, NULL, test_suite_setup, NULL, NULL);

static int mock_power_signal_set_ap_force_shutdown(enum power_signal signal,
						   int value)
{
	if (power_signal_set_fake.call_count == 1) {
		zassert_true(signal == PWR_EC_PCH_RSMRST && value == 1,
			     "First call signal: %d, value: %d", signal, value);
		return 0;
	} else if (power_signal_set_fake.call_count == 2) {
		zassert_true(signal == PWR_EN_PP3300_A && value == 0,
			     "Second call signal: %d, value: %d", signal,
			     value);
		return 0;
	} else if (power_signal_set_fake.call_count == 3) {
		zassert_true(signal == PWR_EN_PP5000_A && value == 0,
			     "Second call signal: %d, value: %d", signal,
			     value);
		return 0;
	}

	zassert_unreachable("Wrong input received");
	return -1;
}

static int
mock_power_signal_get_ap_force_shutdown_retries(enum power_signal signal)
{
	if (signal == PWR_RSMRST_PWRGD) {
		return 1;
	}
	zassert_unreachable("Wrong input received");
	return -1;
}

static int mock_power_signal_get_ap_force_shutdown(enum power_signal signal)
{
	if (signal == PWR_RSMRST_PWRGD) {
		if (power_signal_get_fake.call_count <= 5) {
			return 1;
		}
		return 0;
	}
	zassert_unreachable("Wrong input received");
	return -1;
}

ZTEST_USER(ptl_board_power, test_force_shutdown)
{
	power_signal_set_fake.custom_fake =
		mock_power_signal_set_ap_force_shutdown;
	power_signal_get_fake.custom_fake =
		mock_power_signal_get_ap_force_shutdown;
	board_ap_power_force_shutdown();

	zassert_equal(3, power_signal_set_fake.call_count);
	zassert_equal(7, power_signal_get_fake.call_count);
}

ZTEST_USER(ptl_board_power, test_board_ap_power_force_shutdown_timeout)
{
	power_signal_set_fake.custom_fake =
		mock_power_signal_set_ap_force_shutdown;
	power_signal_get_fake.custom_fake =
		mock_power_signal_get_ap_force_shutdown_retries;

	const uint32_t start_ms = k_uptime_get();

	board_ap_power_force_shutdown();

	const uint32_t end_ms = k_uptime_get();

	zassert_equal(power_signal_set_fake.call_count, 3);
	zassert_true((end_ms - start_ms) >= X86_NON_DSX_FORCE_SHUTDOWN_TO_MS);
	zassert_true(power_signal_get_fake.call_count > 2);
}

ZTEST_USER(ptl_board_power, test_board_power_signal_get)
{
	zassert_equal(0, board_power_signal_get(0));
}

ZTEST_USER(ptl_board_power, test_board_power_signal_set)
{
	zassert_equal(0, board_power_signal_set(0, 0));
}
