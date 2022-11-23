/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include <zephyr/drivers/gpio.h>

#include <ap_power/ap_power.h>
#include <ap_power/ap_power_events.h>
#include <ap_power/ap_power_interface.h>
#include <ap_power_override_functions.h>
#include <power_signals.h>
#include <x86_power_signals.h>

#include "gpio_signal.h"
#include "gpio/gpio.h"

// FAKE_VALUE_FUNC(int, power_signal_set, enum power_signal, int);
// FAKE_VALUE_FUNC(int, power_signal_get, enum power_signal);
//DECLARE_FAKE_VALUE_FUNC(int, power_signal_set, enum power_signal, int);
//DEFINE_FAKE_VALUE_FUNC(int, power_signal_set, enum power_signal, int);
//DECLARE_FAKE_VALUE_FUNC(int, power_signal_get, enum power_signal);
//DEFINE_FAKE_VALUE_FUNC(int, power_signal_get, enum power_signal);

int power_signal_set_mock_ap_force_shutdown(enum power_signal signal, int value)
{
//	if ((signal == PWR_EC_PCH_RSMRST) && (value == 0)) {
//		return 0;
//	} else if ((signal == PWR_EN_PP3300_A) && (value == 0)) {
//		return 0;
//	}

	zassert_unreachable("Wrong input received");
	return -1;
}

int power_signal_set_mock_ap_force_shutdown_rsmrst(enum power_signal signal,
						   int value)
{
//	if ((signal == PWR_EC_PCH_RSMRST) && (value == 0)) {
//		return 0;
//	}

	zassert_unreachable("Wrong input received");
	return -1;
}

int power_signal_get_mock(enum power_signal signal)
{
	return 1;
}

static void board_power_before(void *fixture)
{
	ARG_UNUSED(fixture);
//	RESET_FAKE(power_signal_set);
//	RESET_FAKE(power_signal_get);
}

ZTEST_USER(board_power, test_board_ap_power_force_shutdown)
{
//	power_signal_set.custom_fake = power_signal_set_mock;
//	power_signal_get.custom_fake = power_signal_get_mock;
	board_ap_power_force_shutdown();
}

ZTEST_SUITE(board_power, NULL, NULL, board_power_before, NULL, NULL);
