/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hardware timers driver */

#include "clock.h"
#include "common.h"
#include "hooks.h"
#include "hwtimer.h"
#include "registers.h"
#include "task.h"
#include "timer.h"

void __hw_clock_event_set(uint32_t deadline)
{
}

uint32_t __hw_clock_event_get(void)
{
	return 0xffffffff;
}

void __hw_clock_event_clear(void)
{
}

uint32_t __hw_clock_source_read(void)
{
	return 0xffffffff;
}

void __hw_clock_source_set(uint32_t ts)
{
}

int __hw_clock_source_init(uint32_t start_t)
{
	return 0;
}
