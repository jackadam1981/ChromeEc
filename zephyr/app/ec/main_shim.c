/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_app_main.h"
#include "host_command.h"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(test_log, LOG_LEVEL_INF);

int record_flush;
/** A stub main to call the real ec app main function. LCOV_EXCL_START */
int main(void)
{
	ec_app_main();

	record_flush = 1;
	printk("abc");
	LOG_ERR("No ACPI node with given name");
	//execute shell_print() by cmd "i2c scan i2c@f01c40"
	printk("record_flush %d\n", record_flush);
	record_flush = 0;

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
