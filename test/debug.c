/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "debug.h"
#include "test_util.h"

test_static int test_debugger_is_connected(void)
{
	TEST_EQ(debugger_is_connected(), true, "%d");
	return EC_SUCCESS;
}


void run_test(int argc, char **argv)
{
	test_reset();
	RUN_TEST(test_debugger_is_connected);
	test_print_result();
}
