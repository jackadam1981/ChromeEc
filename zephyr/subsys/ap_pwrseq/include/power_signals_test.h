/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief API for internal tests for the power signals.
 *
 * Extra functions used for testing.
 */

#ifndef __AP_PWRSEQ_POWER_SIGNALS_TEST_H__
#define __AP_PWRSEQ_POWER_SIGNALS_TEST_H__

/**
 * @brief Force an immediate update of the power signals
 *
 * Should only be used for testing.
 */
void force_power_update_signals(void);

/**
 * @brief Wait for the previous request to complete
 *
 * Should only be used for testing.
 */
void wait_power_update_signals(void);

#endif /* __AP_PWRSEQ_POWER_SIGNALS_TEST_H__ */
