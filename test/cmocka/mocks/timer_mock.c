/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "test.h"

#include "timer.h"

#include <stdio.h>

/* Below implementation is based on board/host/timer.c */
static timestamp_t boot_time;

timestamp_t _get_time(void)
{
	static timestamp_t time;

	++time.val;
	return time;
}

timestamp_t get_time(void)
{
	timestamp_t ret = _get_time();

	ret.val -= boot_time.val;
	return ret;
}

uint32_t __hw_clock_source_read(void)
{
	return get_time().le.lo;
}

void force_time(timestamp_t ts)
{
	timestamp_t now = _get_time();

	boot_time.val = now.val - ts.val;
}

void udelay(unsigned int us)
{
	timestamp_t deadline;

	deadline.val = get_time().val + us;
	force_time(deadline);
}
