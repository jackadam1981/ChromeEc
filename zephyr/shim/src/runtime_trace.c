/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "host_command.h"
#include "task.h"
#include "timer.h"
#include "watchdog.h"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/__assert.h>

/*
 * This runtime trace is based on the Zephyr sys_trace API.
 *
 * Assumptions about context switching:
 * - Threads are switched out before a thread is switched in.
 * - A current thread is always running.
 * - ISRs can be nested.
 * - Sleeping is a subset of pending.
 */

#if !defined(CONFIG_ASSERT)
#undef __ASSERT
#define __ASSERT(x, msg, ...) \
	if (!(x))             \
	printk("Assert Error: " msg "\n", ##__VA_ARGS__)
#endif

static struct k_spinlock lock;

struct runtime_stats {
	uint32_t window_a_ticks;
	uint32_t window_b_ticks;
	uint32_t total_ticks;
};

typedef enum {
	THREAD_STATE_INIT,
	THREAD_STATE_RUNNING,
	THREAD_STATE_PENDING,
	THREAD_STATE_SLEEPING,
	THREAD_STATE_QUEUED,
} thread_state_t;

typedef enum {
	CPU_STATE_INIT,
	CPU_STATE_ISR,
	CPU_STATE_THREAD,
	CPU_STATE_SCHEDULER,
} cpu_state_t;

typedef struct {
	thread_state_t state;
	uint32_t last_transition_ts_ticks;
	struct runtime_stats running;
	struct runtime_stats queued;
	struct runtime_stats sleeping;
} thread_stats_t;

static cpu_state_t cpu_state = CPU_STATE_INIT;
static uint32_t cpu_last_transition_ts_ticks = 0;

static struct runtime_stats cpu_isr;
static struct runtime_stats cpu_thread;
static struct runtime_stats cpu_scheduler;

#define CPU_MAX_ISR_NEST 5
static cpu_state_t cpu_prev_state_stack[CPU_MAX_ISR_NEST];

bool window_stats_a_current_b_max = true;

__thread thread_stats_t current_thread_stats;

static uint32_t reset_ts_ticks = 0;

static uint32_t window_start_ts_ticks = 0;
static uint32_t max_window_elapsed_ticks = 0;
static uint32_t max_window_start_ts_ticks = 0;

static uint8_t isr_nest_level = 0;
static uint8_t isr_nest_level_max = 0;
static uint8_t isr_nest_level_max_window_a = 0;
static uint8_t isr_nest_level_max_window_b = 0;

static uint8_t isr_count_total = 0;
static uint8_t isr_count_window_a = 0;
static uint8_t isr_count_window_b = 0;

#if !defined(CONFIG_THREAD_MONITOR)
/* If CONFIG_THREAD_MONITOR is not defined, we need to implement our own thread
 * iteration function.
 *
 * Assumptions:
 * - Thread pointers are aligned to 4 bytes
 * - All thread pointers share the same base address (bits [31:20])
 * - CONFIG_PLATFORM_EC_RUNTIME_TRACE_MAX_THREADS is large enough to hold all
 * threads
 * - sys_trace_thread_create_user atleast once for each thread
 *
 */

#define CONFIG_PLATFORM_EC_RUNTIME_TRACE_MAX_THREADS 20

static uint8_t thread_count;
/* Stores just bits [19:2] of the thread pointer */
static uint16_t thread_index_to_thread_offset
	[CONFIG_PLATFORM_EC_RUNTIME_TRACE_MAX_THREADS];
static uint32_t thread_index_to_thread_offset_base;

#define THREAD_INDEX_TO_THREAD_OFFSET_BASE_MASK 0xFFFC0000
#define THREAD_INDEX_TO_THREAD_OFFSET_MASK 0x3FF

static k_tid_t inline thread_index_to_thread(uint8_t index)
{
	if (index >= ARRAY_SIZE(thread_index_to_thread_offset)) {
		return NULL;
	}
	return (k_tid_t)((uint32_t)thread_index_to_thread_offset_base |
			 (thread_index_to_thread_offset[index] << 2));
}

static int inline thread_to_thread_index(k_tid_t thread)
{
	for (int i = 0;
	     i < MIN(thread_count, ARRAY_SIZE(thread_index_to_thread_offset));
	     i++) {
		if (thread_index_to_thread(i) == thread) {
			return i;
		}
	}
	return -1;
}

void k_thread_foreach_unlocked(k_thread_user_cb_t user_cb, void *user_data)
{
	for (int i = 0;
	     i < MIN(thread_count, ARRAY_SIZE(thread_index_to_thread_offset));
	     i++) {
		k_tid_t thread = thread_index_to_thread(i);
		if (thread != NULL) {
			user_cb(thread, user_data);
		}
	}
}

void sys_trace_thread_create_user(k_tid_t thread)
{
	K_SPINLOCK(&lock)
	{
		if (thread_count == 0) {
			thread_index_to_thread_offset_base =
				(uint32_t)thread &
				THREAD_INDEX_TO_THREAD_OFFSET_BASE_MASK;
		} else {
			/* Thread address bits [31:20] must match base address
			 */
			if (((uint32_t)thread &
			     THREAD_INDEX_TO_THREAD_OFFSET_BASE_MASK) !=
			    thread_index_to_thread_offset_base) {
				__ASSERT(
					0,
					"Thread address bits [31:20] must match base address");
			}
			/* Thread must be aligned to 4 bytes */
			if (((uint32_t)thread) & 0x3) {
				__ASSERT(0,
					 "Thread must be aligned to 4 bytes");
			}
		}
		if (thread_to_thread_index(thread) != -1) {
			return;
		}
		if (thread_count < ARRAY_SIZE(thread_index_to_thread_offset)) {
			thread_index_to_thread_offset[thread_count] =
				(uint32_t)thread >> 2;
		}
		thread_count++;
	}
}

#endif /* !CONFIG_THREAD_MONITOR */

static thread_stats_t *get_thread_stats(const struct k_thread *thread)
{
	thread_stats_t *thread_stats = thread->custom_data;
	if (thread_stats == NULL) {
		return NULL;
	}
	return thread_stats;
}

static thread_state_t get_thread_state(const struct k_thread *thread)
{
	thread_stats_t *thread_stats = get_thread_stats(thread);
	if (thread_stats == NULL) {
		return THREAD_STATE_INIT;
	}
	return thread_stats->state;
}

static void set_thread_state(const struct k_thread *thread,
			     thread_state_t state)
{
	thread_stats_t *thread_stats = get_thread_stats(thread);
	if (thread_stats == NULL) {
		return;
	}
	thread_stats->state = state;
}

static void sync_thread_stats(const struct k_thread *thread,
			      uint32_t now_ts_ticks)
{
	thread_stats_t *thread_stats = get_thread_stats(thread);
	if (thread_stats == NULL) {
		return;
	}
	uint32_t elapsed_ticks =
		(now_ts_ticks - thread_stats->last_transition_ts_ticks);
	thread_stats->last_transition_ts_ticks = now_ts_ticks;

	switch (thread_stats->state) {
	case THREAD_STATE_INIT:
		/* Do not track init time */
		break;
	case THREAD_STATE_RUNNING:
		if (window_stats_a_current_b_max) {
			thread_stats->running.window_a_ticks += elapsed_ticks;
		} else {
			thread_stats->running.window_b_ticks += elapsed_ticks;
		}
		thread_stats->running.total_ticks += elapsed_ticks;
		break;
	case THREAD_STATE_PENDING:
		/* Do not track pending time */
		break;
	case THREAD_STATE_QUEUED:
		if (window_stats_a_current_b_max) {
			thread_stats->queued.window_a_ticks += elapsed_ticks;
		} else {
			thread_stats->queued.window_b_ticks += elapsed_ticks;
		}
		thread_stats->queued.total_ticks += elapsed_ticks;
		break;
	case THREAD_STATE_SLEEPING:
		if (window_stats_a_current_b_max) {
			thread_stats->sleeping.window_a_ticks += elapsed_ticks;
		} else {
			thread_stats->sleeping.window_b_ticks += elapsed_ticks;
		}
		thread_stats->sleeping.total_ticks += elapsed_ticks;
		break;
	default:
		__ASSERT(0, "Invalid thread state");
		break;
	}
}

static void sync_cpu_stats(uint32_t now_ts_ticks)
{
	uint32_t elapsed_ticks = now_ts_ticks - cpu_last_transition_ts_ticks;
	cpu_last_transition_ts_ticks = now_ts_ticks;

	switch (cpu_state) {
	case CPU_STATE_INIT:
		/* Do not track init time */
		break;
	case CPU_STATE_ISR:
		if (window_stats_a_current_b_max) {
			cpu_isr.window_a_ticks += elapsed_ticks;
		} else {
			cpu_isr.window_b_ticks += elapsed_ticks;
		}
		cpu_isr.total_ticks += elapsed_ticks;
		break;
	case CPU_STATE_THREAD:
		if (window_stats_a_current_b_max) {
			cpu_thread.window_a_ticks += elapsed_ticks;
		} else {
			cpu_thread.window_b_ticks += elapsed_ticks;
		}
		cpu_thread.total_ticks += elapsed_ticks;
		break;
	case CPU_STATE_SCHEDULER:
		if (window_stats_a_current_b_max) {
			cpu_scheduler.window_a_ticks += elapsed_ticks;
		} else {
			cpu_scheduler.window_b_ticks += elapsed_ticks;
		}
		cpu_scheduler.total_ticks += elapsed_ticks;
		break;
	default:
		__ASSERT(0, "Invalid cpu_state");
		break;
	}
}

static void reset_thread_current_window_stats(const struct k_thread *thread,
					      void *now_ts_ticks_ptr)
{
	thread_stats_t *thread_stats = get_thread_stats(thread);
	if (thread_stats == NULL) {
		return;
	}
	if (window_stats_a_current_b_max) {
		thread_stats->running.window_a_ticks = 0;
		thread_stats->queued.window_a_ticks = 0;
		thread_stats->sleeping.window_a_ticks = 0;
	} else {
		thread_stats->running.window_b_ticks = 0;
		thread_stats->queued.window_b_ticks = 0;
		thread_stats->sleeping.window_b_ticks = 0;
	}
}

static void handle_thread_window_end(const struct k_thread *thread,
				     void *now_ts_ticks_ptr)
{
	uint32_t now_ts_ticks = *(uint32_t *)now_ts_ticks_ptr;
	sync_thread_stats(thread, now_ts_ticks);
}

static void reset_cpu_current_window_stats(void)
{
	if (window_stats_a_current_b_max) {
		cpu_isr.window_a_ticks = 0;
		cpu_thread.window_a_ticks = 0;
		cpu_scheduler.window_a_ticks = 0;
		isr_count_window_a = 0;
		isr_nest_level_max_window_a = 0;
	} else {
		cpu_isr.window_b_ticks = 0;
		cpu_thread.window_b_ticks = 0;
		cpu_scheduler.window_b_ticks = 0;
		isr_count_window_b = 0;
		isr_nest_level_max_window_b = 0;
	}
}

void runtime_trace_window_end(void)
{
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		uint32_t window_elapsed_ticks =
			now_ts_ticks - window_start_ts_ticks;
		sync_cpu_stats(now_ts_ticks);
		k_thread_foreach_unlocked(handle_thread_window_end,
					  &now_ts_ticks);
		/* Check if this is the longest window so far */
		if (window_elapsed_ticks > max_window_elapsed_ticks) {
			window_stats_a_current_b_max =
				!window_stats_a_current_b_max;
			max_window_elapsed_ticks = window_elapsed_ticks;
			max_window_start_ts_ticks = window_start_ts_ticks;
			printk("\nMax Window: %u ms\n\n",
			       k_ticks_to_ms_floor32(max_window_elapsed_ticks));
		}
		window_start_ts_ticks = now_ts_ticks;
		reset_cpu_current_window_stats();
		k_thread_foreach_unlocked(reset_thread_current_window_stats,
					  &now_ts_ticks);
	}
}

static void handle_thread_transition(const struct k_thread *thread,
				     thread_state_t next_state,
				     uint32_t now_ts_ticks)
{
	__ASSERT(
		get_thread_state(thread) != next_state,
		"Thread next_state(%d) should not be equal to current state (%d)",
		next_state, get_thread_state(thread));
	sync_thread_stats(thread, now_ts_ticks);
	set_thread_state(thread, next_state);
}

static void handle_cpu_transition(cpu_state_t cpu_next_state,
				  uint32_t now_ts_ticks)
{
	/* Should be a new state */
	__ASSERT(
		cpu_state != cpu_next_state,
		"cpu_next_state(%d) should not be equal to current cpu_state (%d)",
		cpu_next_state, cpu_state);
	/* Transitioning from state to next_state */
	sync_cpu_stats(now_ts_ticks);
	// cpu_prev_state = cpu_state;
	cpu_state = cpu_next_state;
}

void sys_trace_isr_enter_user(void)
{
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		isr_nest_level++;
		isr_count_total++;
		if (window_stats_a_current_b_max) {
			isr_count_window_a++;
		} else {
			isr_count_window_b++;
		}
		if (isr_nest_level > isr_nest_level_max) {
			isr_nest_level_max = isr_nest_level;
		}
		if (window_stats_a_current_b_max) {
			if (isr_nest_level > isr_nest_level_max_window_a) {
				isr_nest_level_max_window_a = isr_nest_level;
			}
		} else {
			if (isr_nest_level > isr_nest_level_max_window_b) {
				isr_nest_level_max_window_b = isr_nest_level;
			}
		}
		if (isr_nest_level == 1) {
			__ASSERT(
				cpu_state == CPU_STATE_THREAD ||
					cpu_state == CPU_STATE_SCHEDULER ||
					cpu_state == CPU_STATE_INIT,
				"state should be THREAD or SCHEDULER when ISR first enters, %d",
				cpu_state);

			if (isr_nest_level < CPU_MAX_ISR_NEST) {
				cpu_prev_state_stack[isr_nest_level] =
					cpu_state;
			}

			if (cpu_state == CPU_STATE_THREAD) {
				handle_thread_transition(k_current_get(),
							 THREAD_STATE_QUEUED,
							 now_ts_ticks);
			}
			handle_cpu_transition(CPU_STATE_ISR, now_ts_ticks);
		} else {
			/* If we are nested, we should be in ISR state already.
			 */
			/* We save the state anyway to be symmetric, though it
			 * should be ISR. */
			if (isr_nest_level < CPU_MAX_ISR_NEST) {
				cpu_prev_state_stack[isr_nest_level] =
					cpu_state;
			}

			__ASSERT(cpu_state == CPU_STATE_ISR,
				 "state should be ISR when ISR nests, %d",
				 cpu_state);
			// sync_cpu_stats(now_ts_ticks);
		}
	}
}

void sys_trace_isr_exit_user(void)
{
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		isr_nest_level--;
		__ASSERT(isr_nest_level >= 0, "isr_nest_level underflow %d",
			 isr_nest_level);
		__ASSERT(cpu_state == CPU_STATE_ISR,
			 "state should be ISR when ISR exits, %d", cpu_state);
		if (isr_nest_level == 0) {
			cpu_state_t prev_state = CPU_STATE_INIT;
			if ((isr_nest_level + 1) < CPU_MAX_ISR_NEST) {
				prev_state =
					cpu_prev_state_stack[isr_nest_level + 1];
			} else {
				/* Fallback or default if overflow (unlikely) */
				/* We assume Thread if we lost track, or maybe
				 * we should panic? */
				/* Just keep prev_state as INIT which might be
				 * safe-ish or just wrong. */
				__ASSERT(0, "ISR Nest Level underflow stack");
			}

			if (prev_state == CPU_STATE_THREAD) {
				handle_thread_transition(k_current_get(),
							 THREAD_STATE_RUNNING,
							 now_ts_ticks);
			}
			handle_cpu_transition(prev_state, now_ts_ticks);
		} else {
			/* Nested exit */
			cpu_state_t prev_state = CPU_STATE_ISR;
			if ((isr_nest_level + 1) < CPU_MAX_ISR_NEST) {
				prev_state =
					cpu_prev_state_stack[isr_nest_level + 1];
			}
			/* We should be restoring ISR state */
			__ASSERT(
				prev_state == CPU_STATE_ISR,
				"Nested ISR exit should restore ISR state, got %d",
				prev_state);
			// handle_cpu_transition(prev_state, now_ts_ticks); //
			// Transition ISR->ISR?
			/* If we synced on entry, we would sync here. But we
			 * don't. */
		}
	}
}

void sys_trace_thread_switched_in_user(void)
{
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		/* Always set the custom data for the thread */
		k_current_get()->custom_data = &current_thread_stats;
		__ASSERT(
			isr_nest_level == 0,
			"isr_nest_level should be 0 when thread switches in, %d",
			isr_nest_level);
		__ASSERT(
			cpu_state == CPU_STATE_SCHEDULER ||
				cpu_state == CPU_STATE_ISR,
			"state should be SCHEDULER or ISR when thread switches in, %d",
			cpu_state);
		handle_thread_transition(k_current_get(), THREAD_STATE_RUNNING,
					 now_ts_ticks);
		handle_cpu_transition(CPU_STATE_THREAD, now_ts_ticks);
	}
}

void sys_trace_thread_switched_out_user(void)
{
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		__ASSERT(
			isr_nest_level == 0,
			"isr_nest_level should be 0 when thread switches out, %d",
			isr_nest_level);
		if (get_thread_state(k_current_get()) == THREAD_STATE_RUNNING) {
			handle_thread_transition(k_current_get(),
						 THREAD_STATE_QUEUED,
						 now_ts_ticks);
		}
		handle_cpu_transition(CPU_STATE_SCHEDULER, now_ts_ticks);
	}
}

void sys_trace_thread_sched_ready_user(struct k_thread *thread)
{
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		if (get_thread_state(thread) != THREAD_STATE_QUEUED) {
			handle_thread_transition(thread, THREAD_STATE_QUEUED,
						 now_ts_ticks);
		}
	}
}

void sys_trace_thread_pend_user(struct k_thread *thread)
{
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		if (get_thread_state(thread) != THREAD_STATE_SLEEPING &&
		    get_thread_state(thread) != THREAD_STATE_PENDING) {
			handle_thread_transition(thread, THREAD_STATE_PENDING,
						 now_ts_ticks);
		}
	}
}

void sys_trace_thread_sleep_enter_user(k_timeout_t timeout)
{
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		handle_thread_transition(k_current_get(), THREAD_STATE_SLEEPING,
					 now_ts_ticks);
	}
}

void sys_trace_thread_msleep_enter_user(int ms)
{
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		handle_thread_transition(k_current_get(), THREAD_STATE_SLEEPING,
					 now_ts_ticks);
	}
}

void sys_trace_thread_usleep_enter_user(int us)
{
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		handle_thread_transition(k_current_get(), THREAD_STATE_SLEEPING,
					 now_ts_ticks);
	}
}

static void reset_thread_stats(const struct k_thread *thread,
			       void *now_ts_ticks_ptr)
{
	thread_stats_t *stats = thread->custom_data;
	uint32_t now_ts_ticks = *(uint32_t *)now_ts_ticks_ptr;
	stats->last_transition_ts_ticks = now_ts_ticks;
	memset(&stats->running, 0, sizeof(stats->running));
	memset(&stats->queued, 0, sizeof(stats->queued));
	memset(&stats->sleeping, 0, sizeof(stats->sleeping));
}

static void reset_cpu_stats(uint32_t now_ts_ticks)
{
	cpu_last_transition_ts_ticks = now_ts_ticks;
	memset(&cpu_isr, 0, sizeof(cpu_isr));
	memset(&cpu_scheduler, 0, sizeof(cpu_scheduler));
	memset(&cpu_thread, 0, sizeof(cpu_thread));
	isr_count_window_a = 0;
	isr_nest_level_max_window_a = 0;
	isr_count_window_b = 0;
	isr_nest_level_max_window_b = 0;
	isr_count_total = 0;
	isr_nest_level_max = 0;
}

static void runtime_trace_reset(void)
{
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		reset_ts_ticks = now_ts_ticks;
		window_start_ts_ticks = now_ts_ticks;
		max_window_elapsed_ticks = 0;
		reset_cpu_stats(now_ts_ticks);
		k_thread_foreach_unlocked(reset_thread_stats, &now_ts_ticks);
	}
}

static void dump_ticks(char *label, uint32_t total_ticks, uint32_t ticks)
{
	uint32_t percent = 0;
	if (total_ticks > 0)
		percent = (ticks * 1000) / total_ticks;
	printk("\t%s: %u ms (%u.%u%%)\n", label, k_ticks_to_ms_floor32(ticks),
	       percent / 10, percent % 10);
}

static void dump_thread_window_stats(const struct k_thread *thread,
				     void *total_ticks_ptr)
{
	if (thread == get_idle_thread()) {
		return;
	}
	thread_stats_t *thread_stats = thread->custom_data;
	if (thread_stats == NULL) {
		__ASSERT(false, "Thread %s has no stats",
			 k_thread_name_get((struct k_thread *)thread));
		return;
	}
	uint32_t total_ticks = *(uint32_t *)total_ticks_ptr;
	printk("%s(%d):\n", k_thread_name_get((k_tid_t)thread),
	       k_thread_priority_get((k_tid_t)thread));
	if (window_stats_a_current_b_max) {
		dump_ticks("Running", total_ticks,
			   thread_stats->running.window_b_ticks);
		dump_ticks("Queued", total_ticks,
			   thread_stats->queued.window_b_ticks);
		dump_ticks("Sleeping", total_ticks,
			   thread_stats->sleeping.window_b_ticks);
	} else {
		dump_ticks("Running", total_ticks,
			   thread_stats->running.window_a_ticks);
		dump_ticks("Queued", total_ticks,
			   thread_stats->queued.window_a_ticks);
		dump_ticks("Sleeping", total_ticks,
			   thread_stats->sleeping.window_a_ticks);
	}
}

static void dump_thread_total_stats(const struct k_thread *thread,
				    void *total_ticks_ptr)
{
	thread_stats_t *thread_stats = get_thread_stats(thread);
	if (thread_stats == NULL) {
		return;
	}
	if (thread == get_idle_thread()) {
		return;
	}
	uint32_t total_ticks = *(uint32_t *)total_ticks_ptr;
	printk("%s(%d):\n", k_thread_name_get((k_tid_t)thread),
	       k_thread_priority_get((k_tid_t)thread));
	dump_ticks("Running", total_ticks, thread_stats->running.total_ticks);
	dump_ticks("Queued", total_ticks, thread_stats->queued.total_ticks);
	dump_ticks("Sleeping", total_ticks, thread_stats->sleeping.total_ticks);
}

static void dump_cpu_window_stats(uint32_t total_ticks)
{
	thread_stats_t *idle_stats = get_thread_stats(get_idle_thread());
	if (window_stats_a_current_b_max) {
		dump_ticks("ISR", total_ticks, cpu_isr.window_b_ticks);
		dump_ticks("Scheduler", total_ticks,
			   cpu_scheduler.window_b_ticks);
		dump_ticks("Thread", total_ticks,
			   cpu_thread.window_b_ticks -
				   idle_stats->running.window_b_ticks);
		dump_ticks("Idle", total_ticks,
			   idle_stats->running.window_b_ticks);
		printk("\n");
		printk("ISR: count=%d max_level=%d\n", isr_count_window_b,
		       isr_nest_level_max_window_b);
	} else {
		dump_ticks("ISR", total_ticks, cpu_isr.window_a_ticks);
		dump_ticks("Scheduler", total_ticks,
			   cpu_scheduler.window_a_ticks);
		dump_ticks("Thread", total_ticks,
			   cpu_thread.window_a_ticks -
				   idle_stats->running.window_a_ticks);
		dump_ticks("Idle", total_ticks,
			   idle_stats->running.window_a_ticks);
		printk("\n");
		printk("ISR: count=%d max_level=%d\n", isr_count_window_a,
		       isr_nest_level_max_window_a);
	}
}

static void dump_cpu_total_stats(uint32_t total_ticks)
{
	thread_stats_t *idle_stats = get_thread_stats(get_idle_thread());
	dump_ticks("ISR", total_ticks, cpu_isr.total_ticks);
	dump_ticks("Scheduler", total_ticks, cpu_scheduler.total_ticks);
	dump_ticks("Thread", total_ticks,
		   cpu_thread.total_ticks - idle_stats->running.total_ticks);
	dump_ticks("Idle", total_ticks, idle_stats->running.total_ticks);
	printk("\n");
	printk("ISR: count=%d max_level=%d\n", isr_count_total,
	       isr_nest_level_max);
}

void runtime_trace_dump(void)
{
	watchdog_reload();
	K_SPINLOCK(&lock)
	{
		uint32_t now_ts_ticks = k_uptime_ticks();
		uint32_t since_reset_ticks = now_ts_ticks - reset_ts_ticks;
		uint32_t since_reset_thread_time =
			since_reset_ticks -
			get_thread_stats(get_idle_thread())->running.total_ticks;
		uint32_t max_window_thread_time;
		if (window_stats_a_current_b_max) {
			max_window_thread_time =
				cpu_thread.window_b_ticks -
				get_thread_stats(get_idle_thread())
					->running.window_b_ticks;
		} else {
			max_window_thread_time =
				cpu_thread.window_a_ticks -
				get_thread_stats(get_idle_thread())
					->running.window_a_ticks;
		}

		printk("=== Runtime Stats ===\n");
		printk("CONFIG_SYS_CLOCK_TICKS_PER_SEC=%d\n",
		       CONFIG_SYS_CLOCK_TICKS_PER_SEC);
		printk("CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC=%d\n",
		       CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC);
		printk("\n");
		printk("Reset: %u ms @ %u.%u s\n",
		       k_ticks_to_ms_floor32(since_reset_ticks),
		       k_ticks_to_ms_floor32(reset_ts_ticks) / 1000,
		       k_ticks_to_ms_floor32(reset_ts_ticks) % 1000);
		dump_cpu_total_stats(since_reset_ticks);
		printk("\n");
		k_thread_foreach_unlocked(dump_thread_total_stats,
					  &since_reset_thread_time);
		printk("\n");
		printk("Max: %u ms @ %u.%u s\n",
		       k_ticks_to_ms_floor32(max_window_elapsed_ticks),
		       k_ticks_to_ms_floor32(max_window_start_ts_ticks) / 1000,
		       k_ticks_to_ms_floor32(max_window_start_ts_ticks) % 1000);
		dump_cpu_window_stats(max_window_elapsed_ticks);
		printk("\n");
		k_thread_foreach_unlocked(dump_thread_window_stats,
					  &max_window_thread_time);
	}
	watchdog_reload();
}

int runtime_trace_cmd(const struct shell *shell, size_t argc, char *argv[])
{
	if (argc == 2) {
		if (strcmp(argv[1], "reset") == 0) {
			runtime_trace_reset();
		} else {
			return -EINVAL;
		}
	} else if (argc == 1) {
		runtime_trace_dump();
	} else {
		return -EINVAL;
	}
	return 0;
}

SHELL_CMD_REGISTER(stats, NULL, "stats [reset]", runtime_trace_cmd);
