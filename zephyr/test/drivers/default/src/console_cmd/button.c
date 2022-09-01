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

/* Default button command, no arguments */
ZTEST_USER(console_cmd_button, test_button_no_arg)
{
	int rv;

	rv = shell_execute_cmd(get_ec_shell(), "button");

	zassert_equal(EC_ERROR_PARAM_COUNT, rv, "Expected %d, returned %d",
		      EC_ERROR_PARAM_COUNT, rv);
}

ZTEST_SUITE(console_cmd_button, NULL, NULL, NULL, NULL, NULL);
