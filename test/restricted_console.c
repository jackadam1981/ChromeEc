/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test_util.h"

volatile int is_locked;

int system_is_locked(void)
{
	return is_locked;
}

test_static int test_command_mem_dump(void)
{
	int res;
	char input1[] = "md 0x2000cfd8 2";
	char input2[] = "md 0x2000cf67 2";

	is_locked = 0;
	res = test_send_console_command(input1);
	TEST_EQ(res, EC_SUCCESS, "%d");

	is_locked = 1;
	res = test_send_console_command(input2);
	TEST_EQ(res, EC_ERROR_ACCESS_DENIED, "%d");

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();

	RUN_TEST(test_command_mem_dump);

	test_print_result();
}
