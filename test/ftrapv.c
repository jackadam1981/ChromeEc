/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>

#include "common.h"
#include "panic.h"
#include "system.h"
#include "task.h"
#include "test_util.h"

test_static int test_ftrapv(void)
{
	int32_t test_overflow = INT32_MAX;
	int32_t ret;

	ccprintf("Testing signed integer overflow\n");
	cflush();
	ret = test_overflow + 1;

	/* Should never reach this. */
	ccprintf("ret: %d\n", ret);
	cflush();

	return EC_ERROR_UNKNOWN;
}

test_static int test_panic_data(void)
{
	const uint32_t expected_reason = 0;
	const uint32_t expected_info = 0;
	/*
	 * https://developer.arm.com/documentation/dui0552/a/the-cortex-m3-processor/exception-model/exception-types
	 */
	const uint8_t expected_exception = 6; /* usage fault */

	uint32_t reason = UINT32_MAX;
	uint32_t info = UINT32_MAX;
	uint8_t exception = UINT8_MAX;

	panic_get_reason(&reason, &info, &exception);

	TEST_EQ(reason, expected_reason, "%08x");
	TEST_EQ(info, expected_info, "%d");
	TEST_EQ(exception, expected_exception, "%d");

	return EC_SUCCESS;
}

test_static void run_test_step1(void)
{
	test_set_next_step(TEST_STATE_STEP_2);
	RUN_TEST(test_ftrapv);
}

test_static void run_test_step2(void)
{
	RUN_TEST(test_panic_data);

	if (test_get_error_count())
		test_reboot_to_next_step(TEST_STATE_FAILED);
	else
		test_reboot_to_next_step(TEST_STATE_PASSED);
}

void test_run_step(uint32_t state)
{
	if (state & TEST_STATE_MASK(TEST_STATE_STEP_1)) {
		run_test_step1();
	} else if (state & TEST_STATE_MASK(TEST_STATE_STEP_2)) {
		run_test_step2();
	}
}

int task_test(void *unused)
{
	if (IS_ENABLED(SECTION_IS_RW))
		test_run_multistep();
	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();
	msleep(30); /* Wait for TASK_ID_TEST to initialize */
	task_wake(TASK_ID_TEST);
}
