/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "test.h"

#include "task.h"
#include "timer.h"

uint32_t task_wait_event(int timeout_us)
{
	if (timeout_us > 0)
		udelay(timeout_us);

	return mock_type(uint32_t);
}

uint32_t task_set_event(task_id_t tskid, uint32_t event, int wait)
{
	if (wait)
		return task_wait_event(-1);

	return 0;
}

task_id_t task_get_current(void)
{
	return mock_type(task_id_t);
}

uint32_t *task_get_event_bitmap(task_id_t tskid)
{
	return mock_type(uint32_t *);
}
