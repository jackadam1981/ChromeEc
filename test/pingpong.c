/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tasks for scheduling test.
 */

#include "common.h"
#include "console.h"
#include "task.h"
#include "timer.h"
#include "util.h"

int TaskAbc(void *data)
{
	char letter = (char)(unsigned)data;
	char string[2] = {letter, '\0' };
	task_id_t next = task_get_current() + 1;
	if (next > TASK_ID_TESTC)
		next = TASK_ID_TESTA;

	ccprintf("\n[starting Task %c]\n", letter);
	task_wait_event(-1);

	while (1) {
		ccputs(string);
		cflush();
		task_set_event(next, TASK_EVENT_WAKE, 1);
	}

	return EC_SUCCESS;
}

static int command_run_test(int argc, char **argv)
{
	task_wake(TASK_ID_TESTA);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(runtest, command_run_test,
			NULL, NULL, NULL);
