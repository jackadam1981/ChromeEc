/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"
#include "task.h"

#include <zephyr/kernel.h>

struct host_command *zephyr_find_host_command(int command)
{
	STRUCT_SECTION_FOREACH(host_command, cmd)
	{
		if (cmd->command == command)
			return cmd;
	}

	return NULL;
}

k_tid_t get_main_thread(void)
{
	/* Pointer to the main thread, defined in kernel/init.c */
	extern struct k_thread z_main_thread;

	return &z_main_thread;
}

void host_command_main(void)
{
	k_thread_priority_set(get_main_thread(),
			      EC_TASK_PRIORITY(EC_TASK_HOSTCMD_PRIO));
	k_thread_name_set(get_main_thread(), "HOSTCMD");
	host_command_task(NULL);
}
