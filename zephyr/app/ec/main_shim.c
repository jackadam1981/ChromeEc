/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <kernel.h>
#include "ec_app_main.h"
#include "task.h"

void host_command_task(void *u);

extern struct k_thread z_main_thread;

/** A stub main to call the real ec app main function. LCOV_EXCL_START */
void main(void)
{
	ec_app_main();

	if (IS_ENABLED(CONFIG_TASK_HOSTCMD_THREAD_MAIN)) {
		k_thread_priority_set(&z_main_thread,
				      EC_TASK_PRIORITY(EC_TASK_HOSTCMD_PRIO));
		k_thread_name_set(&z_main_thread, "HOSTCMD*");
		host_command_task(NULL);
	} else if (IS_ENABLED(CONFIG_THREAD_MONITOR)) {
		/*
		 * Avoid returning so that the main stack is displayed by the
		 * "kernel stacks" shell command.
		 */
		k_sleep(K_FOREVER);
	}
}
/* LCOV_EXCL_STOP */
