/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <shell/shell.h>
#include <ztest.h>

#include "console.h"
#include "ec_commands.h"
#include "test/drivers/test_state.h"

ZTEST_SUITE(console_cmd_accelinfo, drivers_predicate_post_main, NULL, NULL,
	    NULL, NULL);

ZTEST_USER(console_cmd_accelinfo, test_too_many_args)
{
	int rv = shell_execute_cmd(get_ec_shell(), "accelinfo arg1 arg2");

	zassert_equal(rv, EC_ERROR_PARAM_COUNT, "Expected %d, but got %d",
		      EC_ERROR_PARAM_COUNT, rv);
}
