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
static struct sloppy_mutex sloppy_mtx;
static int test_level;

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

int mutex_random_task(void *unused)
{
	uint32_t mask;
	char letter = 'A'+(TASK_ID_MTX3A - task_get_current());
	/* wait to be activated */

	/*
	 * This task is used by three different tasks in 3 separate tests. In
	 * order to sequence which test is being run, the static variable
	 * test_level is used. Between the tests a function is called which sets
	 * the test_level variable and wakes each task to break out of the while
	 * loop. This task is structured this way to limit the
	 * number of test tasks that need to be defined. This is necessary as
	 * some test boards are limited on .bss space and the additional stack
	 * space per task pushes over the limit.
	 */

	/* Loop to testing mutex contention */
	while (test_level == 0) {
		task_wait_event(0);
		ccprintf("1%c+\n", letter);
		mutex_lock(&mtx);
		/* Create mask to eliminate lower priority tasks */
		mask = ~((1 << task_get_current()) - 1);
		/* Make sure no higher pri tasks are pending on mutex */
		if (mask & mtx.waiters)
			test_fail();
		ccprintf("1%c=\n", letter);
		task_wait_event(50);
		ccprintf("1%c-\n", letter);
		mutex_unlock(&mtx);
	}

	/* Loop to test sloppy_mutex that allows multiple lock attempts */
	while (test_level == 1) {
		task_wait_event(0);
		ccprintf("2%c+\n", letter);
		sloppy_mutex_lock(&sloppy_mtx);
		/* Create mask to eliminate lower priority tasks */
		mask = ~((1 << task_get_current()) - 1);
		/* Make sure no higher pri tasks are pending on mutex */
		if (mask & sloppy_mtx.waiters)
			test_fail();
		ccprintf("2%c=\n", letter);
		/* Have same task repeat the lock attempt */
		sloppy_mutex_lock(&sloppy_mtx);
		sloppy_mutex_lock(&sloppy_mtx);
		task_wait_event(50);
		ccprintf("2%c-\n", letter);
		sloppy_mutex_unlock(&sloppy_mtx);
	}

	/* Loop for testing random lock order and wait times */
	while (test_level == 2) {
		task_wait_event(0);
		ccprintf("3%c+\n", letter);
		mutex_lock(&mtx);
		ccprintf("3%c=\n", letter);
		task_wait_event(0);
		ccprintf("3%c-\n", letter);
		mutex_unlock(&mtx);
	}

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
	sloppy_mutex_lock(&sloppy_mtx);
	/* Wake 3 tasks which attempt to grab the lock */
	for (n = 0; n < 3; n++) {
		task_id = TASK_ID_MTX3C + ((n + offset) % 3);
		task_wake(task_id);
		/* Update mask of tasks which should be pending on lock */
		task_mask |= 1 << task_id;
		/* Allow contention task to run */
		task_wait_event(10);
	}

	sloppy_mutex_lock(&sloppy_mtx);

	/* Verify that mutex lock is pending for all 3 contention tasks */
	if (sloppy_mtx.waiters != task_mask) {
		ccprintf("fail: task_mask = 0x%x, waiters = 0x%x\n",
			 task_mask, sloppy_mtx.waiters);
		test_fail();
	}
	sloppy_mutex_unlock(&sloppy_mtx);
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
		task_id = TASK_ID_MTX3C + ((n + offset) % 3);
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

static void mutex_advance_test(int level)
{
	int i;

	/* Set test level to allow while loop to exit */
	test_level = level;

	/* Advance each of the 3 instances of random task */
	for (i = 0; i < 3; i++) {
		task_wake(TASK_ID_MTX3C + i);
		task_wait_event(100);
	}
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

	/* Initialize random task while loop entry */
	test_level = 0;

	/* --- Test lock contention and order of task resume --- */
	ccprintf("Test contention and release order\n");
	for (i = 0; i < 3; i++)
		mutex_contention_loop(i);

	/* Allow random task while loop to advance */
	mutex_advance_test(1);

	ccprintf("Test repeat lock attempts\n");
	for (i = 0; i < 3; i++)
		mutex_repeat_loop(0);

	/* Allow random task while loop to advance */
	mutex_advance_test(2);

	/* --- mass lock-unlocking from several tasks --- */
	ccprintf("Massive locking/unlocking :\n");
	for (i = 0; i < 500; i++) {
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
