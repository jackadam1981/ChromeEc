/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_TEST_MOCK_POWER_SIGNALS_H
#define ZEPHYR_TEST_MOCK_POWER_SIGNALS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include <ap_power_override_functions.h>
#include <power_signals.h>
#include <x86_power_signals.h>

typedef uint32_t power_signal_mask_t;

/* Mocks for ec/zephyr/subsys/ap_pwrseq/power_signals.c */
DECLARE_FAKE_VALUE_FUNC(int, power_signal_set, enum power_signal, int);
DECLARE_FAKE_VALUE_FUNC(int, power_signal_get, enum power_signal);
DECLARE_FAKE_VALUE_FUNC(int, power_wait_mask_signals_timeout,
			power_signal_mask_t, power_signal_mask_t, int);

int mock_power_signal_set_ap_force_shutdown(enum power_signal signal,
					    int value);

int mock_power_signal_set_ap_power_action_g3_s5(enum power_signal signal,
						int value);

int mock_power_signal_get_ap_force_shutdown_retries(enum power_signal signal);

int mock_power_signal_get_ap_force_shutdown(enum power_signal signal);

int mock_power_signal_get_check_power_rails_enabled_0(enum power_signal signal);

int mock_power_signal_get_check_power_rails_enabled_1(enum power_signal signal);

int mock_power_wait_mask_signals_timeout_0(power_signal_mask_t want,
					   power_signal_mask_t mask,
					   int timeout);

int mock_power_wait_mask_signals_timeout_1(power_signal_mask_t want,
					   power_signal_mask_t mask,
					   int timeout);
#endif /* ZEPHYR_TEST_MOCK_POWER_SIGNALS_H */
