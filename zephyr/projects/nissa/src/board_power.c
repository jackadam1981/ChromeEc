/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <logging/log.h>
#include <power_signals.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

int board_power_signal_get(enum power_signal signal)
{
	return -EINVAL;
}

int board_power_signal_set(enum power_signal signal, int value)
{
	return -EINVAL;
}
