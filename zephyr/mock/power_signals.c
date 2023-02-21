/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "power_signals.h"
#include "mock/power_signals.h"

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(mock_power_signals);

int power_signal_set_custom_fake(enum power_signal signal, int value)
{
	return 0;
}
int power_signal_get_custom_fake(enum power_signal signal)
{
	return 0;
}
int power_wait_mask_signals_timeout_custom_fake(power_signal_mask_t want,
						power_signal_mask_t mask,
						int timeout)
{
	return 0;
}
