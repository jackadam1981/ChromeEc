/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "task.h"

#define CPRINTS(format, args...) do {\
	cprintf(CC_DEMO, "%s:", task_get_name(task_get_current())); \
	cprintf(CC_DEMO, format, ##args); \
	cprintf(CC_DEMO, "\n"); \
	} while (0)

void demo_task(void *u)
{
	CPRINTS("----%s starting----", __func__);
	while (1) {
		CPRINTS("----waiting someone to wake me up----");
		task_wait_event(-1);
	}
	CPRINTS("----never go to here----");
}
