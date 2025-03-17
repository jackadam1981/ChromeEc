/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "power_button.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

int64_t get_time_dsw_pwrok(void)
{
	/* DSW_PWROK is turned on before EC was powered. */
	return -20 * USEC_PER_MSEC;
}
