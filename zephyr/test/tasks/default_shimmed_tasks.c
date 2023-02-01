/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_tasks.h"
#include "task.h"
#include "timer.h"
#include "zephyr_console_shim.h"

#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>
#include <zephyr/ztest.h>

k_tid_t get_main_thread(void);
k_tid_t get_sysworkq_thread(void);
k_tid_t get_idle_thread(void);

/* Utility functions for finding a Zephyr thread by name */
static k_tid_t found_thread;
static void find_thread_by_name_cb(const struct k_thread *thread,
				   void *user_data)
{
	const char *name = (const char *)user_data;

	if (strcmp(thread->name, name) == 0)
		found_thread = (k_tid_t)thread;
}

static k_tid_t find_thread_by_name(const char *name)
{
	found_thread = NULL;
	k_thread_foreach_unlocked(find_thread_by_name_cb, (void *)name);
	return found_thread;
}

static void test_main_thread_to_task_mapping(void)
{
	k_tid_t main_thread;

	main_thread = find_thread_by_name("main");
	zassert_not_null(main_thread);
	zassert_equal(main_thread, get_main_thread());
	zassert_equal(TASK_ID_MAIN, thread_id_to_task_id(main_thread));
	zassert_equal(task_id_to_thread_id(TASK_ID_MAIN), main_thread);
}

static void test_sysworkq_thread_to_task_mapping(void)
{
	k_tid_t sysworkq_thread;

	sysworkq_thread = find_thread_by_name("sysworkq");
	zassert_not_null(sysworkq_thread);
	zassert_equal(sysworkq_thread, get_sysworkq_thread());
	zassert_equal(TASK_ID_SYSWORKQ, thread_id_to_task_id(sysworkq_thread));
	zassert_equal(task_id_to_thread_id(TASK_ID_SYSWORKQ), sysworkq_thread);
}

static void test_idle_thread_to_task_mapping(void)
{
	k_tid_t idle_thread;

	idle_thread = find_thread_by_name("idle");
	zassert_not_null(idle_thread);
	zassert_equal(idle_thread, get_idle_thread());
	zassert_equal(TASK_ID_IDLE, thread_id_to_task_id(idle_thread));
	zassert_equal(task_id_to_thread_id(TASK_ID_IDLE), idle_thread);
}

static void test_shell_thread_to_task_mapping(void)
{
	k_tid_t shell_thread;

	shell_thread = find_thread_by_name("shell_uart");
	zassert_not_null(shell_thread);
	zassert_equal(shell_thread, get_shell_thread());
	zassert_equal(TASK_ID_SHELL, thread_id_to_task_id(shell_thread));
	zassert_equal(task_id_to_thread_id(TASK_ID_SHELL), shell_thread);
}

void test_main(void)
{
	/* Note that test_set_event_before_task_start calls start_ec_tasks */
	ztest_test_suite(test_shimmed_tasks,
			 ztest_unit_test(test_main_thread_to_task_mapping),
			 ztest_unit_test(test_sysworkq_thread_to_task_mapping),
			 ztest_unit_test(test_idle_thread_to_task_mapping),
			 ztest_unit_test(test_shell_thread_to_task_mapping));
	ztest_run_test_suite(test_shimmed_tasks);
}
