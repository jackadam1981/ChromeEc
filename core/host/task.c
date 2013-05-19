/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Task scheduling / events module for Chrome EC operating system */

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>

#include "atomic.h"
#include "common.h"
#include "task.h"
#include "task_id.h"
#include "test_util.h"
#include "timer.h"

struct emu_task_t {
	pthread_t thread;
	pthread_cond_t resume;
	uint32_t event;
	timestamp_t wake_time;
};

struct task_args {
	void (*routine)(void *);
	void *d;
};

static struct emu_task_t tasks[TASK_ID_COUNT];
static pthread_cond_t scheduler_cond;
static pthread_mutex_t run_lock;
static task_id_t running_task_id;

static sem_t interrupt_sem;
static pthread_mutex_t interrupt_lock;
static pthread_t interrupt_thread;
static int in_interrupt;
static int interrupt_disabled;
static void (*pending_isr)(void);

static __thread task_id_t my_task_id; /* thread local task id */

#define TASK(n, r, d, s) void r(void *);
CONFIG_TASK_LIST
CONFIG_TEST_TASK_LIST
#undef TASK

/* Idle task */
void __idle(void *d)
{
	while (1)
		task_wait_event(-1);
}

void _run_test(void *d)
{
	run_test();
}

#define TASK(n, r, d, s) {r, d},
struct task_args task_info[TASK_ID_COUNT] = {
	{__idle, NULL},
	CONFIG_TASK_LIST
	CONFIG_TEST_TASK_LIST
	{_run_test, NULL},
};
#undef TASK

#define TASK(n, r, d, s) #n,
static const char * const task_names[] = {
	"<< idle >>",
	CONFIG_TASK_LIST
	CONFIG_TEST_TASK_LIST
	"<< test runner >>",
};
#undef TASK

void task_pre_init(void)
{
	/* Nothing */
}

int in_interrupt_context(void)
{
	return !!in_interrupt;
}

void interrupt_disable(void)
{
	interrupt_disabled = 1;
}

void interrupt_enable(void)
{
	interrupt_disabled = 0;
}

void _task_execute_isr(int sig)
{
	in_interrupt = 1;
	pending_isr();
	sem_post(&interrupt_sem);
	in_interrupt = 0;
}

void task_register_interrupt(void)
{
	sem_init(&interrupt_sem, 0, 0);
	signal(SIGUSR1, _task_execute_isr);
}

void task_trigger_test_interrupt(void (*isr)(void))
{
	if (interrupt_disabled)
		return;
	pthread_mutex_lock(&interrupt_lock);

	/* Suspend current task and excute ISR */
	pending_isr = isr;
	pthread_kill(tasks[running_task_id].thread, SIGUSR1);

	/* Wait for ISR to complete */
	sem_wait(&interrupt_sem);
	while (in_interrupt)
		;
	pending_isr = NULL;

	pthread_mutex_unlock(&interrupt_lock);
}

uint32_t task_set_event(task_id_t tskid, uint32_t event, int wait)
{
	tasks[tskid].event = event;
	if (wait)
		return task_wait_event(-1);
	return 0;
}

uint32_t task_wait_event(int timeout_us)
{
	int tid = task_get_current();
	int ret;
	pthread_mutex_lock(&interrupt_lock);
	if (timeout_us > 0)
		tasks[tid].wake_time.val = get_time().val + timeout_us;

	/* Transfer control to scheduler */
	pthread_cond_signal(&scheduler_cond);
	pthread_cond_wait(&tasks[tid].resume, &run_lock);

	/* Resume */
	ret = tasks[tid].event;
	tasks[tid].event = 0;
	pthread_mutex_unlock(&interrupt_lock);
	return ret;
}

void mutex_lock(struct mutex *mtx)
{
	int value = 0;
	int id = 1 << task_get_current();

	mtx->waiters |= id;

	do {
		if (mtx->lock == 0) {
			mtx->lock = 1;
			value = 1;
		}

		if (!value)
			task_wait_event(-1);
	} while (!value);

	mtx->waiters &= ~id;
}

void mutex_unlock(struct mutex *mtx)
{
	int v;
	mtx->lock = 0;

	for (v = 31; v >= 0; --v)
		if ((1ul << v) & mtx->waiters) {
			mtx->waiters &= ~(1ul << v);
			task_set_event(v, TASK_EVENT_MUTEX, 0);
			break;
		}
}

task_id_t task_get_current(void)
{
	return my_task_id;
}

void task_scheduler(void)
{
	int i;
	timestamp_t now;

	while (1) {
		now = get_time();
		i = TASK_ID_COUNT - 1;
		while (i >= 0) {
			if (tasks[i].event || now.val >= tasks[i].wake_time.val)
				break;
			--i;
		}
		if (i < 0)
			i = TASK_ID_IDLE;

		tasks[i].wake_time.val = ~0ull;
		running_task_id = i;
		pthread_cond_signal(&tasks[i].resume);
		pthread_cond_wait(&scheduler_cond, &run_lock);
	}
}

void *_task_start_impl(void *a)
{
	long tid = (long)a;
	struct task_args *arg = task_info + tid;
	my_task_id = tid;
	pthread_mutex_lock(&run_lock);

	/* Wait for scheduler */
	task_wait_event(1);
	tasks[tid].event = 0;

	/* Start the task routine */
	(arg->routine)(arg->d);

	/* Catch exited routine */
	while (1)
		task_wait_event(-1);
}

void __interrupt_generator(void)
{
}

void interrupt_generator(void)
	__attribute__((weak, alias("__interrupt_generator")));

void *_task_int_generator_start(void *d)
{
	interrupt_generator();
	return NULL;
}

int task_start(void)
{
	int i;

	task_register_interrupt();

	pthread_mutex_init(&run_lock, NULL);
	pthread_mutex_init(&interrupt_lock, NULL);
	pthread_cond_init(&scheduler_cond, NULL);

	pthread_mutex_lock(&run_lock);

	for (i = 0; i < TASK_ID_COUNT; ++i) {
		tasks[i].event = TASK_EVENT_WAKE;
		tasks[i].wake_time.val = ~0ull;
		pthread_cond_init(&tasks[i].resume, NULL);
		pthread_create(&tasks[i].thread, NULL, _task_start_impl,
			       (void *)(uintptr_t)i);
		pthread_cond_wait(&scheduler_cond, &run_lock);
		/*
		 * Interrupt lock is grabbed by the task which just started.
		 * Let's unlock it so the next task can be started.
		 */
		pthread_mutex_unlock(&interrupt_lock);
	}

	/*
	 * All tasks are now waiting in task_wait_event(). Lock interrupt_lock
	 * here so the first task chosen sees it locked.
	 */
	pthread_mutex_lock(&interrupt_lock);

	pthread_create(&interrupt_thread, NULL,
		       _task_int_generator_start, NULL);

	task_scheduler();

	return 0;
}
