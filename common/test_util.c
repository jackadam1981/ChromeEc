/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test utilities.
 */

#include "console.h"
#include "test_util.h"
#include "util.h"

int __test_error_count;

#ifdef TEST_COVERAGE
extern void __gcov_flush(void);
#else
static void __gcov_flush(void) {}
#endif

/* Weak reference function as an entry point for unit test */
test_mockable void run_test(void) { }

void test_reset(void)
{
	__test_error_count = 0;
}

void test_pass(void)
{
	__gcov_flush();
	ccprintf("Pass!\n");
}

void test_fail(void)
{
	__gcov_flush();
	ccprintf("Fail!\n");
}

void test_print_result(void)
{
	__gcov_flush();
	if (__test_error_count)
		ccprintf("Fail! (%d tests)\n", __test_error_count);
	else
		ccprintf("Pass!\n");
}

int test_get_error_count(void)
{
	return __test_error_count;
}

static int command_run_test(int argc, char **argv)
{
	run_test();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(runtest, command_run_test,
			NULL, NULL, NULL);
