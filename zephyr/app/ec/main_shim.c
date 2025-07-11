/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_app_main.h"
#include "host_command.h"

#include <zephyr/kernel.h>

#ifdef CONFIG_PLATFORM_EC_HOST_INTERFACE_HECI
extern void command_idle_stats(void);
static struct k_timer d0ix_status_timer;

static void print_d0ix_info(struct k_timer *timer)
{
	command_idle_stats();
}
#endif


/** A stub main to call the real ec app main function. LCOV_EXCL_START */
int main(void)
{
	ec_app_main();

#ifdef CONFIG_PLATFORM_EC_HOST_INTERFACE_HECI
	k_timer_init(&d0ix_status_timer, print_d0ix_info, NULL);
 	k_timer_start(&d0ix_status_timer, K_MSEC(20000), K_MSEC(20000));
#endif

	if (IS_ENABLED(CONFIG_TASK_HOSTCMD_THREAD_MAIN)) {
		host_command_main();
	} else if (IS_ENABLED(CONFIG_THREAD_MONITOR)) {
		/*
		 * Avoid returning so that the main stack is displayed by the
		 * "kernel stacks" shell command.
		 */
		k_sleep(K_FOREVER);
	}

	return 0;
}
/* LCOV_EXCL_STOP */
