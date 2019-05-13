/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "common.h"
#include "console.h"
#include "watchdog.h"

/* Common functionality for watchdog timer */
static uint32_t reset_counter;

/*
 * The reset counter may only be in the range [0, m], where m is
 * CONFIG_WATCHDOG_MAX_RETRIES. Use this function to ensure the
 * getters and setters always return a valid value.
 */
static inline uint32_t reset_counter_check(uint32_t value)
{
	if (value <= CONFIG_WATCHDOG_MAX_RETRIES)
		return value;
	ccprints("Warning: invalid value %u", value);
	return 0;
}

void watchdog_set_reset_counter(uint32_t value)
{
	reset_counter = reset_counter_check(value);
	ccprints("reset_counter set to %u", reset_counter);
}

uint32_t watchdog_get_reset_counter(void)
{
	ccprints("reset_counter got as %u", reset_counter);
	return reset_counter_check(reset_counter);
}
