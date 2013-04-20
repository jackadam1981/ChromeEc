/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Watchdog common code */

#include "common.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "watchdog.h"

void watchdog_trace(struct panic_data *pdata)
{
	report_exception(pdata);
	/* If we are blocked in a high priority IT handler, the following debug
	 * messages might not appear but they are useless in that situation. */
	timer_print_info();
	task_print_list();
}
