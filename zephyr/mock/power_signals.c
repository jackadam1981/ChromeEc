/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/logging/log.h>

#include <mock/power_signals.h>

LOG_MODULE_REGISTER(mock_power_signals);

int mock_power_signal_set_ap_force_shutdown(enum power_signal signal, int value)
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
	}

	zassert_unreachable("Wrong input received");
	return -1;
}

int mock_power_signal_set_ap_power_action_g3_s5(enum power_signal signal,
						int value)
{
	if ((signal == PWR_EN_PP3300_A) && (value == 1)) {
		return 0;
	}

	zassert_unreachable("Wrong input received");
	return -1;
}

int mock_power_signal_get_ap_force_shutdown_retries(enum power_signal signal)
{
	if (signal == PWR_RSMRST_PWRGD) {
		return 1;
	}
	zassert_unreachable("Wrong input received");
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
	zassert_unreachable("Wrong input received");
	return -1;
}

int mock_power_signal_get_check_power_rails_enabled_0(enum power_signal signal)
{
	if (signal == PWR_EN_PP3300_A) {
		return 0;
	}
	zassert_unreachable("Wrong input received");
	return -1;
}

int mock_power_signal_get_check_power_rails_enabled_1(enum power_signal signal)
{
	if (signal == PWR_EN_PP3300_A) {
		return 1;
	}
	zassert_unreachable("Wrong input received");
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