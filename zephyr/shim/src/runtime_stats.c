/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Captures runtime statistics for threads and the system.
 *
 * The runtime statistics are captured using the Zephyr object core statistics
 * API. This API has a lower overhead compared to THREAD_ANALYZER,
 * THREAD_USAGE_ANALYSIS, or CPU_LOAD.
 *
 * The runtime statistics are captured for all threads. The total runtime
 * statistics are inferred from the sum of all thread runtime statistics.
 * The idle thread is used to the total load. This implementation assumes a
 * single CPU core.
 */

#include "ec_commands.h"
#include "host_command.h"
#include "runtime_stats.h"
#include "task.h"

#include <zephyr/arch/arch_interface.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

static struct k_spinlock lock;

#ifdef CONFIG_PLATFORM_EC_HOSTCMD_RUNTIME_STATS
struct thread_index_to_thread_data {
	int16_t target_index;
	int16_t current_index;
	k_tid_t thread;
};

static void thread_index_to_thread_cb(const struct k_thread *thread,
				      void *_data)
{
	struct thread_index_to_thread_data *data =
		(struct thread_index_to_thread_data *)_data;
	if (data->current_index++ == data->target_index) {
		data->thread = (k_tid_t)thread;
	}
}

static void thread_count_cb(const struct k_thread *thread, void *_count)
{
	int16_t *count = (int16_t *)_count;
	(*count)++;
}

static void runtime_stats_stop_thread_cb(const struct k_thread *thread,
					 void *unused)
{
	k_thread_runtime_stats_disable((k_tid_t)thread);
}

static void runtime_stats_start_thread_cb(const struct k_thread *thread,
					  void *unused)
{
	k_thread_runtime_stats_enable((k_tid_t)thread);
}

static enum ec_status
host_command_runtime_stats_sys(struct host_cmd_handler_args *args)
{
	struct ec_request_runtime_stats_sys *req =
		(struct ec_request_runtime_stats_sys *)args->params;
	struct ec_response_runtime_stats_sys *resp =
		(struct ec_response_runtime_stats_sys *)args->response;
	struct k_thread_runtime_stats stats;
	uint64_t current_time_cycles;
	K_SPINLOCK(&lock)
	{
		if (req->stop_tracking_threads) {
			k_thread_foreach_unlocked(runtime_stats_stop_thread_cb,
						  NULL);
		}
		if (req->stop_tracking_sys) {
			k_sys_runtime_stats_disable();
		}
		current_time_cycles = k_cycle_get_64();
		k_thread_runtime_stats_all_get(&stats);

		resp->current_time_us = k_cyc_to_us_near64(current_time_cycles);
		resp->exec_us = k_cyc_to_us_near64(stats.execution_cycles);
		resp->idle_us = k_cyc_to_us_near64(stats.idle_cycles);

		resp->num_threads = 0;
		k_thread_foreach_unlocked(thread_count_cb, &resp->num_threads);
		args->response_size = sizeof(*resp);

		if (req->start_tracking_threads) {
			k_thread_foreach_unlocked(runtime_stats_start_thread_cb,
						  NULL);
		}
		if (req->start_tracking_sys) {
			k_sys_runtime_stats_enable();
		}
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_RUNTIME_STATS_SYS, host_command_runtime_stats_sys,
		     EC_VER_MASK(0));

static enum ec_status
host_command_runtime_stats_thread(struct host_cmd_handler_args *args)
{
	struct ec_request_runtime_stats_thread *req =
		(struct ec_request_runtime_stats_thread *)args->params;
	struct ec_response_runtime_stats_thread *resp =
		(struct ec_response_runtime_stats_thread *)args->response;
	struct k_thread_runtime_stats stats;
	uint64_t current_time_cycles;
	k_tid_t thread;
	K_SPINLOCK(&lock)
	{
		struct thread_index_to_thread_data data = {
			.target_index = req->thread_index,
			.current_index = 0,
			.thread = NULL,
		};
		k_thread_foreach_unlocked(thread_index_to_thread_cb, &data);
		if (data.thread == NULL) {
			return EC_RES_INVALID_PARAM;
		}
		thread = data.thread;
		if (req->stop_tracking) {
			k_thread_runtime_stats_disable(thread);
		}
		current_time_cycles = k_cycle_get_64();
		k_thread_runtime_stats_get(thread, &stats);

		resp->current_time_us = k_cyc_to_us_near64(current_time_cycles);
		resp->exec_us = k_cyc_to_us_near64(stats.execution_cycles);
		resp->peak_us = k_cyc_to_us_near64(stats.peak_cycles);
		/* The API doesn't provide the number of windows, so it's
		 * reversed from the average and total cycles */
		resp->num_windows = stats.total_cycles / stats.average_cycles;
		resp->thread_id = (uint32_t)thread;
		resp->is_idle_thread = thread == get_idle_thread();
		args->response_size = sizeof(*resp);

		if (req->reset_peak) {
			k_thread_runtime_stats_longest_frame_reset(thread);
		}
		if (req->start_tracking) {
			k_thread_runtime_stats_enable(thread);
		}
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_RUNTIME_STATS_THREAD,
		     host_command_runtime_stats_thread, EC_VER_MASK(0));

#endif

#ifdef CONFIG_PLATFORM_EC_CONSOLE_CMD_RUNTIME_STATS

static void dump_thread_runtime_stats_cb(const struct k_thread *thread,
					 void *_elapsed_cycles)
{
	uint64_t elapsed_cycles = *(uint64_t *)_elapsed_cycles;
	if (IS_ENABLED(CONFIG_THREAD_NAME))
		printk("\n%s:\n", k_thread_name_get((k_tid_t)thread));
	else
		printk("\n%p:\n", thread);

	struct k_thread_runtime_stats stats;
	k_thread_runtime_stats_get((k_tid_t)thread, &stats);
	printk("Total: %llu cycles %llu ms (%llu%%)\n", stats.total_cycles,
	       k_cyc_to_ms_near64(stats.total_cycles),
	       stats.total_cycles * 100 / elapsed_cycles);
	printk("Peak: %llu cycles %llu ms (%llu%%)\n", stats.peak_cycles,
	       k_cyc_to_ms_near64(stats.peak_cycles),
	       stats.peak_cycles * 100 / elapsed_cycles);
	printk("Avg: %llu cycles %llu ms (%llu%%)\n", stats.average_cycles,
	       k_cyc_to_ms_near64(stats.average_cycles),
	       stats.average_cycles * 100 / elapsed_cycles);
	printk("Num Windows: %u (%llu)\n", thread->base.usage.num_windows,
	       stats.total_cycles / stats.average_cycles);
	k_thread_runtime_stats_longest_frame_reset((k_tid_t)thread);
}

int cmd_runtime_stats_dump(const struct shell *shell, size_t argc,
			   size_t argv[])
{
	struct k_thread_runtime_stats all_stats;
	uint64_t now_cycles;
	uint64_t other_cycles;

	K_SPINLOCK(&lock)
	{
		/* Stop tracking current thread so usage not counted while
		 * dumping */
		if (IS_ENABLED(CONFIG_SCHED_THREAD_USAGE_ANALYSIS)) {
			k_thread_runtime_stats_disable(k_current_get());
		}
		now_cycles = k_cycle_get_64();
		k_thread_runtime_stats_all_get(&all_stats);
		printk("=== Runtime Stats ===\n");
		printk("Now: %llu ms\n", k_cyc_to_ms_near64(now_cycles));
		printk("Idle: %llu ms (%llu%%)\n",
		       k_cyc_to_ms_near64(all_stats.idle_cycles),
		       all_stats.idle_cycles * 100 / now_cycles);
		/* total_cycles is the sum of idle and non-idle cycles */
		printk("Non-Idle: %llu ms (%llu%%)\n",
		       k_cyc_to_ms_near64(all_stats.total_cycles),
		       all_stats.total_cycles * 100 / now_cycles);
		/* execution_cycles is the sum of idle and non-idle cycles */
		printk("Total: %llu ms (%llu%%)\n",
		       k_cyc_to_ms_near64(all_stats.execution_cycles),
		       all_stats.execution_cycles * 100 / now_cycles);
		/* other_cycles is the sum of idle and non-idle cycles */
		other_cycles = now_cycles - all_stats.execution_cycles;
		printk("Other: %llu ms (%llu%%)\n",
		       k_cyc_to_ms_near64(other_cycles),
		       other_cycles * 100 / now_cycles);
		k_thread_foreach_unlocked(dump_thread_runtime_stats_cb,
					  &now_cycles);

		if (IS_ENABLED(CONFIG_SCHED_THREAD_USAGE_ANALYSIS)) {
			k_thread_runtime_stats_enable(k_current_get());
		}
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_runtime_stats_cmds,
			       SHELL_CMD_ARG(dump, NULL, "Dump runtime stats",
					     cmd_runtime_stats_dump, 1, 0),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(stats, &sub_runtime_stats_cmds, "", NULL);

#endif
