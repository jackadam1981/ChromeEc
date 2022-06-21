/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <stdint.h>
#include <zephyr/zephyr.h>

#include "hwtimer.h"

uint64_t __hw_clock_source_read64(void)
{
	return k_ticks_to_us_floor64(k_uptime_ticks());
}

uint32_t __hw_clock_event_get(void)
{
	/*
	 * CrOS EC event deadlines don't quite make sense in Zephyr
	 * terms.  Evaluate what to do about this later...
	 */
	return 0;
}

/*
 * This Zephyr version of udelay only impacts the current thread.
 */
void udelay(unsigned us)
{
	k_busy_wait(us);
}

/*
 * This is intended as a busy-wait delay i.e the thread
 * should not yield.
 * The waitms console command uses this to force a watchdog.
 */
void udelay_busy_wait(unsigned us)
{
	uint64_t target = __hw_clock_source_read64() + us;

	k_sched_lock();
	while (__hw_clock_source_read64() < target)
		;
	k_sched_unlock();
}
