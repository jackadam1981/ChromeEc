/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Timer module */

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#include "task.h"
#include "timer.h"

void usleep(unsigned us)
{
	task_wait_event(us);
}

timestamp_t get_time(void)
{
	struct timeval tv;
	timestamp_t ret;
	gettimeofday(&tv, NULL);
	ret.val = 1000000 * (uint64_t)tv.tv_sec + tv.tv_usec;
	return ret;
}

void udelay(unsigned us)
{
	timestamp_t deadline = get_time();
	deadline.val += us;
	while (get_time().val < deadline.val)
		;
}

int timestamp_expired(timestamp_t deadline, const timestamp_t *now)
{
	timestamp_t now_val;

	if (!now) {
		now_val = get_time();
		now = &now_val;
	}

	return ((int64_t)(now->val - deadline.val) >= 0);
}

void timer_init(void)
{
	/* Nothing */
}
