/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_app_main.h"
#include "host_command.h"

#include <zephyr/kernel.h>

/** A stub main to call the real ec app main function. LCOV_EXCL_START */
#ifdef CONFIG_ZEPHYR_PIGWEED_MODULE
int main(void)
#else
void main(void)
#endif
{
	ec_app_main();

	if (IS_ENABLED(CONFIG_TASK_HOSTCMD_THREAD_MAIN)) {
		host_command_main();
	} else if (IS_ENABLED(CONFIG_THREAD_MONITOR)) {
		/*
		 * Avoid returning so that the main stack is displayed by the
		 * "kernel stacks" shell command.
		 */
		k_sleep(K_FOREVER);
	}
#ifdef CONFIG_ZEPHYR_PIGWEED_MODULE
	return 0;
#endif
}
/* LCOV_EXCL_STOP */
