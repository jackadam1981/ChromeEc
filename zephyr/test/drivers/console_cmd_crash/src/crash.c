/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "builtin/assert.h"
#include "console.h"
#include "test/drivers/test_mocks.h"
#include "test/drivers/test_state.h"

#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(console_cmd_crash, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);

ZTEST_USER(console_cmd_crash, test_no_args)
{
	int rv = shell_execute_cmd(get_ec_shell(), "crash");

	zassert_equal(EC_ERROR_PARAM1, rv, "Expected %d, but got %d",
		      EC_ERROR_PARAM1, rv);
}

ZTEST_USER(console_cmd_crash, test_too_many_args)
{
	int rv;

	/* CONFIG_PLATFORM_EC_CONSOLE_CMD_CRASH_NESTED should be disabled for this suite.*/
	zassert_false(IS_ENABLED(CONFIG_PLATFORM_EC_CONSOLE_CMD_CRASH_NESTED));

	rv = shell_execute_cmd(get_ec_shell(), "crash assert asset");

	zassert_equal(EC_ERROR_PARAM_COUNT, rv, "Expected %d, but got %d",
		      EC_ERROR_PARAM_COUNT, rv);
}

ZTEST_USER(console_cmd_crash, test_assert)
{
	int rv;

	RESET_FAKE(assert_post_action);
	rv = shell_execute_cmd(get_ec_shell(), "crash assert");

	zassert_equal(EC_ERROR_UNKNOWN, rv, NULL);
	zassert_equal(1, assert_post_action_fake.call_count, NULL);
}
