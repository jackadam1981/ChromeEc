/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "debug.h"
#include "string.h"
#include "test_util.h"

static bool debugger_connected;
static bool debugger_connected_previously /* = false */;

static void print_usage(void)
{
	ccprintf("usage: runtest [debugger|no_debugger]\n");
}

test_static int test_debugger_is_connected(void)
{
	ccprintf("debugger_is_connected: %d\n", debugger_connected);
	TEST_EQ(debugger_is_connected(), debugger_connected, "%d");
	return EC_SUCCESS;
}

/*
 * The only way to test this is to do the following in the specific order:
 * 1. power cycle the board (if you flashed using debugger or already ran test)
 * 2. runtest no_debugger
 * 3. runtest debugger
 *
 * Note that a reset will not suffice. It must be a power cycle.
 */
test_static int test_debugger_was_connected(void)
{
	ccprintf("debugger_was_connected: %d\n", debugger_connected_previously);
	TEST_EQ(debugger_was_connected(), debugger_connected_previously, "%d");
	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();

	if (argc < 2) {
		print_usage();
		test_fail();
		return;
	}

	if (strncmp(argv[1], "debugger", sizeof("debugger")) == 0) {
		debugger_connected = true;
		debugger_connected_previously = true;
	} else if (strncmp(argv[1], "no_debugger", sizeof("no_debugger")) ==
		   0) {
		debugger_connected = false;
	} else {
		print_usage();
		test_fail();
		return;
	}

	RUN_TEST(test_debugger_is_connected);
	RUN_TEST(test_debugger_was_connected);
	test_print_result();
}

static int command_debugger_check(int argc, const char **argv)
{
	ccprintf("debugger_is_connected() = %d\n", debugger_is_connected());
	ccprintf("debugger_was_connected() = %d\n", debugger_was_connected());
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(debugger, command_debugger_check, "",
			"Check detected debugger status.");
