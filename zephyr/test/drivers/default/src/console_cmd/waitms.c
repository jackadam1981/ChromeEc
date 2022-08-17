/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

#include "console.h"
#include "timer.h"

static void test_duration(unsigned int ms)
{
	char cmd[32];
	timestamp_t start;
	timestamp_t end;

	sprintf(cmd, "waitms %u", ms);
	start = get_time();
	zassert_ok(shell_execute_cmd(get_ec_shell(), cmd), NULL);
	end = get_time();
	zassert_true((end.val - start.val) / 1000 == ms, NULL);
}

ZTEST_SUITE(console_cmd_waitms, NULL, NULL, NULL, NULL, NULL);

ZTEST_USER(console_cmd_waitms, test_waitms)
{
	/*
	 * Test across three orders of magnitude. Beyond ~3s the watchdog will
	 * trigger so don't need to bother testing 10s of seconds or greater.
	 */
	test_duration(5);
	test_duration(75);
	test_duration(250);
	test_duration(1000);
}
