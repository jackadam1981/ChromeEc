/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <kernel.h>
#include <stdint.h>
#include <zephyr.h>

#include "host_command.h"
#include "task.h"

uint32_t task_wait_event(int timeout_us)
{
	if (timeout_us <= 0) {
		k_yield();
		return 0;
	}

	k_usleep(timeout_us);
	return TASK_EVENT_TIMER;
}

int extpower_is_present(void)
{
	return 1;
}

int lid_is_open(void)
{
	return 1;
}

void host_set_single_event(enum host_event_code event)
{
	printk("HOST SET EVENT %d\n", event);
}
