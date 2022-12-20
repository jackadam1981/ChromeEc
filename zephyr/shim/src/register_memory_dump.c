/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "host_command_memory_dump.h"
#include "string.h"

#include <zephyr/arch/cpu.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>

static bool thread_is_excluded_from_memory_dump(const struct k_thread *thread)
{
	/* List of sensitive threads that must be excluded from memory dump */
	static const char * const memory_dump_exclude_threads[] = {
		"KEYSCAN",
		"KEYPROTO",
	};
	for (int i = 0; i < ARRAY_SIZE(memory_dump_exclude_threads); i++)
		if (strcmp(thread->name, memory_dump_exclude_threads[i]) == 0)
			return true;
	return false;
}

static void register_thread_memory_dump_cb(const struct k_thread *thread,
					   void *user_data)
{
	ARG_UNUSED(user_data);

	if (thread_is_excluded_from_memory_dump(thread))
		return;

	register_memory_dump(thread->stack_info.start,
			     thread->stack_info.size -
				     thread->stack_info.delta);
}

int register_thread_memory_dump(void)
{
	k_thread_foreach_unlocked(register_thread_memory_dump_cb, NULL);
	return EC_SUCCESS;
}
