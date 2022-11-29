/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "stddef.h"
#include "console.h"
#include "common.h"
#include "cpu.h"
#include "ec_commands.h"
#include "panic.h"
#include "rw_safe_mode.h"
#include "hooks.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "watchdog.h"

static bool in_rw_safe_mode;

/* List of safe mode critical tasks */
static task_id_t safe_mode_critical_tasks[] = {
	TASK_ID_HOOKS,
	TASK_ID_IDLE,
	TASK_ID_HOSTCMD,
};

static int safe_mode_allowed_hostcmds[] = {
	EC_CMD_SYSINFO,	       EC_CMD_GET_PROTOCOL_INFO,
	EC_CMD_GET_VERSION,    EC_CMD_CONSOLE_SNAPSHOT,
	EC_CMD_CONSOLE_READ,   EC_CMD_GET_NEXT_EVENT,
	EC_CMD_GET_UPTIME_INFO
};

static bool task_is_rw_safe_mode_critical(task_id_t task_id)
{
	for (int i = 0; i < ARRAY_SIZE(safe_mode_critical_tasks); i++)
		if (safe_mode_critical_tasks[i] == task_id)
			return true;
	return false;
}

static void disable_non_critical_tasks(void)
{
	for (task_id_t task_id = 0; task_id < TASK_ID_COUNT; task_id++) {
		if (!task_is_rw_safe_mode_critical(task_id)) {
			task_disable_task(task_id);
		}
	}
}

static void safe_mode_timeout(void)
{
	panic_printf("Safe mode timeout after %d msec\n",
		     SAFE_MODE_TIMEOUT_MSEC);
	panic_reboot();
}
DECLARE_DEFERRED(safe_mode_timeout);

static void schedule_safe_mode_timeout(void)
{
	hook_call_deferred(&safe_mode_timeout_data,
			   SAFE_MODE_TIMEOUT_MSEC * MSEC);
}

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

int start_rw_safe_mode(void)
{
	if (!system_is_in_rw()) {
		ccprints("Can only enter safe mode from RW image\n");
		return EC_ERROR_INVAL;
	}

	if (system_is_in_rw_safe_mode()) {
		panic_printf("Already in rw safe mode");
		return EC_ERROR_INVAL;
	}

	if (task_is_rw_safe_mode_critical(task_get_current())) {
		/* TODO: Restart critical tasks */
		panic_printf(
			"Fault in critical task, cannot enter safe mode\n");
		return EC_ERROR_INVAL;
	}

	disable_non_critical_tasks();

	schedule_safe_mode_timeout();

	in_rw_safe_mode = true;

	panic_printf("\nStarting RW Safe Mode\n");

	return EC_SUCCESS;
}
