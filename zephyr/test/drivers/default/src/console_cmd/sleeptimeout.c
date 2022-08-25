/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

#include "console.h"
#include "ec_commands.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

ZTEST_USER(console_cmd_sleeptimeout, test_no_params)
{
	int rv = shell_execute_cmd(get_ec_shell(), "sleeptimeout");

	zassert_equal(EC_SUCCESS, rv, "Expected %d, but got %d", EC_SUCCESS,
		      rv);
}

ZTEST_USER(console_cmd_sleeptimeout, test_good_params)
{
	zassert_ok(shell_execute_cmd(get_ec_shell(), "sleeptimeout default"),
		   "Failed default print");
	zassert_ok(shell_execute_cmd(get_ec_shell(), "sleeptimeout infinite"),
		   "Failed default print");
	zassert_ok(shell_execute_cmd(get_ec_shell(), "sleeptimeout 1500"),
		   "Failed default print");
}

ZTEST_USER(console_cmd_sleeptimeout, test_bad_params)
{
	int rv = shell_execute_cmd(get_ec_shell(), "sleeptimeout 0");

	zassert_equal(rv, EC_ERROR_PARAM1, "Expected %d, but got %d",
		      EC_ERROR_PARAM1, rv);
}

ZTEST_SUITE(sleeptimeout, NULL, NULL, NULL, NULL, NULL);
