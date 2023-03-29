/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "ec_commands.h"
#include "host_command.h"
#include "test/drivers/test_mocks.h"
#include "test/drivers/test_state.h"
#include "trng.h"

#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>

ZTEST_USER(random, test_hostcmd_rand)
{
	uint32_t rand_response[5];
	struct ec_params_rand_num params = {
		.num_rand_bytes = 16,
	};
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND(
		EC_CMD_RAND_NUM, EC_VER_RAND_NUM, rand_response, params);

	/*
	 * It's necessary to set response_max, because 'ec_response_rand_num'
	 * structure has flexible array member.
	 */
	args.response_max = 20;
	system_is_locked_fake.return_val = 0;

	zassert_ok(host_command_process(&args), NULL);
	zassert_equal(args.response_size, params.num_rand_bytes);
	/* Check that last 4 bytes are 0. */
	zassert_equal(rand_response[4], 0x0, "found 0x%lx", rand_response[4]);
	zassert_equal(system_is_locked_fake.call_count, 1);
}

ZTEST_USER(random, test_hostcmd_rand_overflow)
{
	uint8_t rand_response[16];
	struct ec_params_rand_num params = {
		.num_rand_bytes = 16,
	};
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND(
		EC_CMD_RAND_NUM, EC_VER_RAND_NUM, rand_response, params);

	/* Change maximum response size to small value. */
	args.response_max = 8;
	system_is_locked_fake.return_val = 0;

	zassert_equal(host_command_process(&args), EC_RES_OVERFLOW);
}

ZTEST_USER(random, test_hostcmd_rand_access_denied)
{
	uint8_t rand_response[16];
	struct ec_params_rand_num params = {
		.num_rand_bytes = 16,
	};

	system_is_locked_fake.return_val = 1;

	zassert_equal(
		ec_cmd_rand_num(NULL, &params,
				(struct ec_response_rand_num *)rand_response),
		EC_RES_ACCESS_DENIED, NULL);
	zassert_equal(system_is_locked_fake.call_count, 1);
}

ZTEST_USER(random, test_console_cmd_rand)
{
	const struct shell *shell_zephyr = get_ec_shell();
	const char *outbuffer;
	size_t buffer_size;

	shell_backend_dummy_clear_output(shell_zephyr);

	zassert_ok(shell_execute_cmd(shell_zephyr, "rand"));
	outbuffer = shell_backend_dummy_get_output(shell_zephyr, &buffer_size);

	/*
	 * Output has "rand 64_random_characters" format, so buffer should have
	 * at least 69 characters.
	 */
	zassert_true(buffer_size >= 69, "buffer size is %d", buffer_size);
	zassert_not_null(strstr(outbuffer, "rand "));
}

ZTEST_USER(random, test_trng_rand)
{
	trng_rand();
}

ZTEST_SUITE(random, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
