/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 * Copyright 2011 Google Inc.
 *
 * Tasks for mutexes basic tests.
 */

#include "console.h"
#include "common.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

static struct mutex mtx;
static struct task_mutex mtx_repeat;

/* period between 50us and 3.2ms */
#define PERIOD_US(num) (((num % 64) + 1) * 50)
/* one of the 3 MTX3x tasks */
#define RANDOM_TASK(num) (TASK_ID_MTX3C + (num % 3))

int mutex_multiple_task(void *unused)
{
	task_id_t id = task_get_current();

	ccprintf("\n[Mutex multiple task %d]\n", id);

	task_wait_event(0);
	ccprintf("MTX6: locking...");
	mutex_lock(&mtx);
	ccprintf("done\n");
	mutex_lock(&mtx);
	ccprintf("MTX2: unlocking...\n");
	mutex_unlock(&mtx);

	task_wait_event(0);

	return EC_SUCCESS;
}

int mutex_repeat_task(void *unused)
{
	uint32_t mask;
	char letter = 'A'+(TASK_ID_MTX5A - task_get_current());
	/* wait to be activated */

#if 1
	while (1) {
		task_wait_event(0);
		ccprintf("%c+\n", letter);
		task_mutex_lock(&mtx_repeat);
		/* Create mask to eliminate lower priority tasks */
		mask = ~((1 << task_get_current()) - 1);
		/* Make sure no higher pri tasks are pending on mutex */
		if (mask & mtx_repeat.waiters)
			test_fail();
		ccprintf("%c=\n", letter);
		/* Have same task repeat the lock attempt */
		ccprintf("Attempting lock again\n");
		task_mutex_lock(&mtx_repeat);
		task_mutex_lock(&mtx_repeat);
		task_wait_event(50);
		ccprintf("%c-\n", letter);
		task_mutex_unlock(&mtx_repeat);
	}

	return EC_SUCCESS;
}


int mutex_contention_task(void *unused)
{
	uint32_t mask;
	char letter = 'A'+(TASK_ID_MTX4A - task_get_current());
	/* wait to be activated */

	while (1) {
		task_wait_event(0);
		ccprintf("%c+\n", letter);
		mutex_lock(&mtx);
		/* Create mask to eliminate lower priority tasks */
		mask = ~((1 << task_get_current()) - 1);
		/* Make sure no higher pri tasks are pending on mutex */
		if (mask & mtx.waiters)
			test_fail();
		ccprintf("%c=\n", letter);
		task_wait_event(50);
		ccprintf("%c-\n", letter);
		mutex_unlock(&mtx);
	}

	return EC_SUCCESS;
}

int mutex_random_task(void *unused)
{
	char letter = 'A'+(TASK_ID_MTX3A - task_get_current());
	/* wait to be activated */

	while (1) {
		task_wait_event(0);
		ccprintf("%c+\n", letter);
		mutex_lock(&mtx);
		ccprintf("%c=\n", letter);
		task_wait_event(0);
		ccprintf("%c-\n", letter);
		mutex_unlock(&mtx);
	}

	task_wait_event(0);

	return EC_SUCCESS;
}

int mutex_second_task(void *unused)
{
	task_id_t id = task_get_current();

	ccprintf("\n[Mutex second task %d]\n", id);

	task_wait_event(0);
	ccprintf("MTX2: locking...");
	mutex_lock(&mtx);
	ccprintf("done\n");
	task_wake(TASK_ID_MTX1);
	/* Allow MTX1 task to run to create lock contention */
	task_wait_event(50);
	ccprintf("MTX2: unlocking...\n");
	mutex_unlock(&mtx);

	task_wait_event(0);

	return EC_SUCCESS;
}

static void mutex_repeat_loop(int offset)
{
	int n;
	uint32_t task_mask = 0;
	task_id_t task_id;

	/* Grab the lock */
	task_mutex_lock(&mtx_repeat);
	/* Wake 3 tasks which attempt to grab the lock */
	for (n = 0; n < 3; n++) {
		task_id = TASK_ID_MTX5C + ((n + offset) % 3);
		task_wake(task_id);
		/* Update mask of tasks which should be pending on lock */
		task_mask |= 1 << task_id;
		/* Allow contention task to run */
		task_wait_event(10);
	}

	task_mutex_lock(&mtx_repeat);

	/* Verify that mutex lock is pending for all 3 contention tasks */
	if (mtx_repeat.waiters != task_mask) {
		ccprintf("fail: task_mask = 0x%x, waiters = 0x%x\n",
			 task_mask, mtx.waiters);
		test_fail();
	}
	task_mutex_unlock(&mtx_repeat);
	/* Allow contention tasks to run now that lock is available */
	task_wait_event(1000);
}

static void mutex_contention_loop(int offset)
{
	int n;
	uint32_t task_mask = 0;
	task_id_t task_id;

	/* Grab the lock */
	mutex_lock(&mtx);
	/* Wake 3 tasks which attempt to grab the lock */
	for (n = 0; n < 3; n++) {
		task_id = TASK_ID_MTX4C + ((n + offset) % 3);
		task_wake(task_id);
		/* Update mask of tasks which should be pending on lock */
		task_mask |= 1 << task_id;
		/* Allow contention task to run */
		task_wait_event(10);
	}

	/* Verify that mutex lock is pending for all 3 contention tasks */
	if (mtx.waiters != task_mask)
		test_fail();
	mutex_unlock(&mtx);
	/* Allow contention tasks to run now that lock is available */
	task_wait_event(1000);
}

int mutex_main_task(void *unused)
{
	task_id_t id = task_get_current();
	uint32_t rdelay = (uint32_t)0x0bad1dea;
	uint32_t rtask = (uint32_t)0x1a4e1dea;
	int i;

	ccprintf("\n[Mutex main task %d]\n", id);

	task_wait_event(0);

	/* --- Lock/Unlock without contention --- */
	ccprintf("No contention :");
	mutex_lock(&mtx);
	mutex_unlock(&mtx);
	mutex_lock(&mtx);
	mutex_unlock(&mtx);
	mutex_lock(&mtx);
	mutex_unlock(&mtx);
	ccprintf("done.\n");

	/* --- Serialization to test simple contention --- */
	ccprintf("Simple contention :\n");
	/* lock the mutex from the other task */
	task_set_event(TASK_ID_MTX2, TASK_EVENT_WAKE, 1);
	/* block on the mutex */
	ccprintf("MTX1: blocking...\n");
	mutex_lock(&mtx);
	ccprintf("MTX1: get lock\n");
	mutex_unlock(&mtx);

	/* --- Test lock contention and order of task resume --- */
	ccprintf("Test contention and release order\n");
	for (i = 0; i < 3; i++)
		mutex_contention_loop(i);

	ccprintf("Test repeat lock attempts\n");
	for (i = 0; i < 3; i++)
		mutex_repeat_loop(0);

	/* --- mass lock-unlocking from several tasks --- */
	ccprintf("Massive locking/unlocking :\n");
	for (i = 0; i < 5; i++) {
		/* Wake up a random task */
		task_wake(RANDOM_TASK(rtask));
		/* next pseudo random task */
		rtask = prng(rtask);
		/* Wait for a "random" period */
		task_wait_event(PERIOD_US(rdelay));
		/* next pseudo random delay */
		rdelay = prng(rdelay);
	}

	test_pass();
	task_wait_event(0);

	return EC_SUCCESS;
}

void run_test(void)
{
	wait_for_task_started();
	task_wake(TASK_ID_MTX1);
}
