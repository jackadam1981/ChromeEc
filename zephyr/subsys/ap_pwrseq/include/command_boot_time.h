// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __COMMAND_BOOT_TIME_H__
#define __COMMAND_BOOT_TIME_H__

#include <stdint.h>

static const uint64_t uninitialized_time = -1;

/**
 * @brief Get the time first time on_new_signal_or_state(state, X) is called.
 *
 * @param state_name: The query state. Only support S0 and S5 now.
 * @param output_time: The output time.
 * @return EC_ERROR_PARAM1 if state_name is not supported.
 */
int ap_power_get_boot_time(const char *state_name, uint64_t *output_time);

#ifdef CONFIG_ZTEST
/**
 * @brief Called when state change in pwrseq main loop.
 *
 * @param state: The new state.
 */
void ap_power_test_on_new_state(const char *state_name);

/**
 * @brief Reset boot time for testing.
 */
void ap_power_reset_boot_time();
#endif /* CONFIG_ZTEST */
#endif /* __COMMAND_BOOT_TIME_H__ */
