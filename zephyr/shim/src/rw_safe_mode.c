/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/arch/cpu.h>
#include <zephyr/kernel/thread.h>
#include <zephyr/kernel.h>

#include "ec_tasks.h"
#include "panic.h"
#include "rw_safe_mode.h"
#include "string.h"
#include "system.h"

static bool in_rw_safe_mode;

/* List of safe mode critical tasks */
static char *safe_mode_critical_threads[] = {
	"main",
	"sysworkq",
	"idle",
	"HOSTCMD",
};

static int safe_mode_allowed_hostcmds[] = {
	EC_CMD_SYSINFO,	       EC_CMD_GET_PROTOCOL_INFO,
	EC_CMD_GET_VERSION,    EC_CMD_CONSOLE_SNAPSHOT,
	EC_CMD_CONSOLE_READ,   EC_CMD_GET_NEXT_EVENT,
	EC_CMD_GET_UPTIME_INFO
};

#ifdef TEST_BUILD
void set_safe_mode(bool mode)
{
	in_rw_safe_mode = mode;
}
#endif /* TEST_BUILD */

bool system_is_in_rw_safe_mode(void)
{
	return !!in_rw_safe_mode;
}

bool command_is_allowed_in_rw_safe_mode(int command)
{
	for (int i = 0; i < ARRAY_SIZE(safe_mode_allowed_hostcmds); i++)
		if (command == safe_mode_allowed_hostcmds[i])
			return true;
	return false;
}

bool thread_is_rw_safe_mode_critical(const struct k_thread *thread)
{
	for (int i = 0; i < ARRAY_SIZE(safe_mode_critical_threads); i++)
		if (strcmp(thread->name, safe_mode_critical_threads[i]) == 0)
			return true;
	return false;
}

void abort_non_critical_threads_cb(const struct k_thread *thread,
				   void *user_data)
{
	ARG_UNUSED(user_data);

	/*
	 * Don't abort if thread is critical or current thread.
	 * Current thread will be canceled automatically after returning from
	 * exception handler.
	 */
	if (thread_is_rw_safe_mode_critical(thread) ||
	    k_current_get() == thread)
		return;

	printk("Aborting thread %s\n", thread->name);
	k_thread_abort((struct k_thread *)thread);
}

void abort_non_critical_tasks(void)
{
	k_thread_foreach(abort_non_critical_threads_cb, NULL);
}

void safe_mode_timeout_cb(struct k_timer *unused)
{
	printk("Safe mode timeout after %d msec\n", SAFE_MODE_TIMEOUT_MSEC);
	system_reset(0);
}
K_TIMER_DEFINE(safe_mode_timeout, safe_mode_timeout_cb, NULL);

void schedule_safe_mode_timeout(void)
{
	k_timer_start(&safe_mode_timeout, K_MSEC(SAFE_MODE_TIMEOUT_MSEC),
		      K_NO_WAIT);
}

int start_rw_safe_mode(void)
{
	if (!system_is_in_rw()) {
		printk("Can only enter safe mode from RW image\n");
		return EC_ERROR_INVAL;
	}

	if (system_is_in_rw_safe_mode()) {
		printk("Already in rw safe mode");
		return EC_ERROR_INVAL;
	}

	if (thread_is_rw_safe_mode_critical(k_current_get())) {
		/* TODO: Restart critical tasks */
		printk("Fault in critical task, cannot enter safe mode\n");
		return EC_ERROR_INVAL;
	}

	abort_non_critical_tasks();

	schedule_safe_mode_timeout();

	in_rw_safe_mode = true;

	printk("Starting RW Safe Mode\n");

	return EC_SUCCESS;
}
