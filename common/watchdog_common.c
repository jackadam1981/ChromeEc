/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "link_defs.h"
#include "timer.h"
#include "watchdog.h"

void watchdog_task(void)
{
	while (1) {
		watchdog_reload();
		task_wait_event(HOOK_TICK_INTERVAL);
	}
}
