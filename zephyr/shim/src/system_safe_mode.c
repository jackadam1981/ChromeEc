/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_tasks.h"
#include "panic.h"
#include "string.h"
#include "system.h"
#include "system_safe_mode.h"

#include <zephyr/arch/cpu.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>

__override int disable_non_safe_mode_critical_tasks(void)
{
	for (task_id_t task_id = 0; task_id < TASK_ID_COUNT + EXTRA_TASK_COUNT;
	     task_id++) {
		k_tid_t thread_id = task_id_to_thread_id(task_id);

		if (thread_id == NULL)
			/* Failed to look up thread from task id, skip it */
			continue;
		if (thread_id == k_current_get())
			/* Don't abort current thread, it will be aborted
			 * automatically.
			 */
			continue;
		if (is_task_safe_mode_critical(task_id))
			/* Don't abort critical threads */
			continue;
		k_thread_abort(thread_id);
	}
	return EC_SUCCESS;
}

static void safe_mode_timeout_cb(struct k_timer *unused)
{
	handle_system_safe_mode_timeout();
}
K_TIMER_DEFINE(safe_mode_timeout, safe_mode_timeout_cb, NULL);

__override int schedule_system_safe_mode_timeout(void)
{
	k_timer_start(&safe_mode_timeout,
		      K_MSEC(CONFIG_PLATFORM_EC_SYSTEM_SAFE_MODE_TIMEOUT_MSEC),
		      K_NO_WAIT);
	return EC_SUCCESS;
}
