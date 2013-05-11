/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test hooks.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

static int init_hook_count;
static int tick_hook_count;
static int tick2_hook_count;
static int tick_count_seen_by_tick2;
static int second_hook_count;
static int deferred_call_count;

static void init_hook(void)
{
	init_hook_count++;
}
DECLARE_HOOK(HOOK_INIT, init_hook, HOOK_PRIO_DEFAULT);

static void tick_hook(void)
{
	tick_hook_count++;
}
DECLARE_HOOK(HOOK_TICK, tick_hook, HOOK_PRIO_DEFAULT);

static void tick2_hook(void)
{
	tick2_hook_count++;
	tick_count_seen_by_tick2 = tick_hook_count;
}
DECLARE_HOOK(HOOK_TICK, tick2_hook, HOOK_PRIO_DEFAULT+1);

static void second_hook(void)
{
	second_hook_count++;
}
DECLARE_HOOK(HOOK_SECOND, second_hook, HOOK_PRIO_DEFAULT);

static void deferred_func(void)
{
	deferred_call_count++;
}
DECLARE_DEFERRED(deferred_func);

static int test_init(void)
{
	TEST_ASSERT(init_hook_count == 1);
	return EC_SUCCESS;
}

static int test_ticks(void)
{
	int start_tick = tick_hook_count;
	int start_second = second_hook_count;

	usleep(1000 * MSEC);

	TEST_ASSERT(tick_hook_count == start_tick +
		    1000 * MSEC / HOOK_TICK_INTERVAL);
	TEST_ASSERT(second_hook_count == start_second + 1);

	return EC_SUCCESS;
}

static int test_priority(void)
{
	usleep(HOOK_TICK_INTERVAL);
	TEST_ASSERT(tick_hook_count == tick2_hook_count);
	TEST_ASSERT(tick_hook_count == tick_count_seen_by_tick2);

	return EC_SUCCESS;
}

static int test_deferred(void)
{
	deferred_call_count = 0;
	hook_call_deferred(deferred_func, 10 * MSEC);
	usleep(11 * MSEC);
	TEST_ASSERT(deferred_call_count == 1);

	hook_call_deferred(deferred_func, 10 * MSEC);
	usleep(5 * MSEC);
	hook_call_deferred(deferred_func, -1);
	usleep(10 * MSEC);
	TEST_ASSERT(deferred_call_count == 1);

	hook_call_deferred(deferred_func, 10 * MSEC);
	usleep(5 * MSEC);
	hook_call_deferred(deferred_func, -1);
	usleep(3 * MSEC);
	hook_call_deferred(deferred_func, 5 * MSEC);
	usleep(6 * MSEC);
	TEST_ASSERT(deferred_call_count == 2);

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_init);
	RUN_TEST(test_ticks);
	RUN_TEST(test_priority);
	RUN_TEST(test_deferred);

	test_print_result();
}
