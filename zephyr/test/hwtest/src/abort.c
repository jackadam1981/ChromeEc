/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "panic.h"
#include "system.h"
#include "test_util.h"
#include "zephyr/kernel.h"

#include <stdlib.h>

#include <zephyr/ztest.h>

enum {
	/* Random number to signal the next stage of the test */
	TEST_STATE_ABORT = 0xA76C,
};

static void *abort_setup(void)
{
	return NULL;
}

void abort_teardown(void *fixture)
{
	system_set_scratchpad(0);
}

ZTEST_SUITE(abort, NULL, abort_setup, NULL, NULL, abort_teardown);

static void test_abort(void)
{
	system_set_scratchpad(TEST_STATE_ABORT);
	printk("Calling abort\n");
	cflush();
	abort();
	/* Should never reach this. */
	zassert_unreachable();
}

static void test_panic_data(void)
{
	uint32_t reason = 0;
	uint32_t info = 0;
	uint8_t exception = UINT8_MAX;

	panic_get_reason(&reason, &info, &exception);

	/* Common implementation of abort calls k_panic. */
	zassert_equal(reason, K_ERR_KERNEL_PANIC, "Incorrect reason: 0x%x",
		      reason);
	zassert_equal(info, PANIC_INFO_ZEPHYR_MAGIC, "Incorrect info: 0x%x",
		      info);
	zassert_equal(exception, 0, "Incorrect exception 0x%x", exception);
}

ZTEST(abort, test_abort)
{
	uint32_t state = 0;

	system_get_scratchpad(&state);
	switch (state) {
	case TEST_STATE_ABORT:
		test_panic_data();
		break;
	default:
		test_abort();
	}
}

static void test_thread(void *arg1, void *arg2, void *arg3)
{
	uint32_t state = 0;

	system_get_scratchpad(&state);
	/* The first state is run via console */
	switch (state) {
	case TEST_STATE_ABORT:
		ztest_run_test_suites(NULL, false, 1, 1);
		break;
	default:
		break;
	}
}
K_THREAD_DEFINE(test_thread_tid, 1024, test_thread, NULL, NULL, NULL, 1, 0, 0);
