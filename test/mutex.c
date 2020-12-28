/* Copyright 2011 The Chromium OS Authors. All rights reserved.
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

static mutex_t mtx;

/* period between 50us and 3.2ms */
#define PERIOD_US(num) (((num % 64) + 1) * 50)
/* one of the 3 MTX3x tasks */
#define RANDOM_TASK(num) (TASK_ID_MTX3C + (num % 3))

EC_TEST_RETURN mutex_random_task(TASK_PARAMS)
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

EC_TEST_RETURN mutex_second_task(TASK_PARAMS)
{
	task_id_t id = task_get_current();

	ccprintf("\n[Mutex second task %d]\n", id);

	task_wait_event(0);
	ccprintf("MTX2: locking...");
	mutex_lock(&mtx);
	ccprintf("done\n");
	task_wake(TASK_ID_MTX1);
	ccprintf("MTX2: unlocking...\n");
	mutex_unlock(&mtx);

	task_wait_event(0);

	return EC_SUCCESS;
}

EC_TEST_RETURN mutex_main_task(TASK_PARAMS)
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
	task_set_event(TASK_ID_MTX2, TASK_EVENT_WAKE);
	task_wait_event(0);
	/* block on the mutex */
	ccprintf("MTX1: blocking...\n");
	mutex_lock(&mtx);
	ccprintf("MTX1: get lock\n");
	mutex_unlock(&mtx);

	/* --- mass lock-unlocking from several tasks --- */
	ccprintf("Massive locking/unlocking :\n");
	for (i = 0; i < 500; i++) {
		/* Wake up a random task */
		task_wake(RANDOM_TASK(rtask));
		/* next pseudo random delay */
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

/*
 * Define the test cases to run. We need to do this twice, once in the format
 * that Ztest uses, and again in the format the the EC test framework uses.
 * If you add a test to one of them, make sure to add it to the other.
 */

#ifdef CONFIG_ZEPHYR
static EC_TEST_RETURN test_mutex_main(void)
{
	task_wake(TASK_ID_MTX1);
	task_wait_event(0);

	return EC_SUCCESS;
}

void test_main(void)
{
	/*
	 * When running under the EC kernel, there is a test task that calls
	 * `run_test`, and the other tasks in the list get started as part of
	 * the task list that includes `run_test`. For Zephyr, we are running
	 * in the main thread and must call `start_ec_tasks` explicitly.
	 */
	start_ec_tasks();
	ztest_test_suite(test_mutex_lib,
			 ztest_unit_test(test_mutex_main));
	ztest_run_test_suite(test_mutex_lib);
}
#else
void run_test(int argc, char **argv)
{
	wait_for_task_started();
	task_wake(TASK_ID_MTX1);
}
#endif /* CONFIG_ZEPHYR */
