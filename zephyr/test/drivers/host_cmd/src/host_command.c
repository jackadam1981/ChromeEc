/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>

#include "ec_commands.h"
#include "host_command.h"
#include "test/drivers/test_state.h"

ZTEST(host_cmd_host_commands, read_test)
{
	struct ec_response_read_test response;
	struct ec_params_read_test params = {
		.offset = 1,
		.size = 4 * sizeof(uint32_t),
	};
	uint32_t expected[] = { 1, 2, 3, 4 };
	int rv;

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_READ_TEST, 0, response, params);

	/* This will fail since we didn't set response_size */
	rv = host_command_process(&args);
	zassert_equal(EC_RES_ERROR, rv, "Got %d", rv);

	/* Give the host command some room to write a response and try again */
	args.response_size = sizeof(response);

	rv = host_command_process(&args);

	zassert_ok(rv, "Got %d", rv);
	zassert_mem_equal((uint8_t *)expected, response.data, sizeof(expected));
}

ZTEST(host_cmd_host_commands, get_command_versions__v0)
{
	struct ec_response_get_cmd_versions response;
	struct ec_params_get_cmd_versions params = {
		.cmd = EC_CMD_GET_CMD_VERSIONS
	};
	int rv;

	struct host_cmd_handler_args args = BUILD_HOST_COMMAND(
		EC_CMD_GET_CMD_VERSIONS, 0, response, params);

	rv = host_command_process(&args);

	zassert_ok(rv, "Got %d", rv);
	zassert_equal(EC_VER_MASK(0) | EC_VER_MASK(1), response.version_mask);
}

ZTEST(host_cmd_host_commands, get_command_versions__v1)
{
	struct ec_response_get_cmd_versions response;
	struct ec_params_get_cmd_versions_v1 params = {
		.cmd = EC_CMD_GET_CMD_VERSIONS
	};
	int rv;

	struct host_cmd_handler_args args = BUILD_HOST_COMMAND(
		EC_CMD_GET_CMD_VERSIONS, 1, response, params);

	rv = host_command_process(&args);

	zassert_ok(rv, "Got %d", rv);
	zassert_equal(EC_VER_MASK(0) | EC_VER_MASK(1), response.version_mask);
}

ZTEST(host_cmd_host_commands, get_command_versions__invalid_cmd)
{
	struct ec_response_get_cmd_versions response;
	struct ec_params_get_cmd_versions_v1 params = {
		/* Host command doesn't exist */
		.cmd = UINT16_MAX,
	};
	int rv;

	struct host_cmd_handler_args args = BUILD_HOST_COMMAND(
		EC_CMD_GET_CMD_VERSIONS, 1, response, params);

	rv = host_command_process(&args);

	zassert_equal(EC_RES_INVALID_PARAM, rv, "Got %d", rv);
}

ZTEST(host_cmd_host_commands, get_comms_status)
{
	struct ec_response_get_comms_status response;
	int rv;

	struct host_cmd_handler_args args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_GET_COMMS_STATUS, 0, response);

	rv = host_command_process(&args);

	zassert_ok(rv, "Got %d", rv);

	/* Unit test host commands are processed synchronously, so always expect
	 * the EC to be not busy processing another.
	 */
	zassert_false(response.flags);
}

ZTEST(host_cmd_host_commands, resend_response)
{
	struct host_cmd_handler_args args =
		(struct host_cmd_handler_args)BUILD_HOST_COMMAND_SIMPLE(
			EC_CMD_RESEND_RESPONSE, 0);
	int rv;

	rv = host_command_process(&args);
	zassert_ok(rv);

	/* The way we trigger host commands in tests doesn't cause results to
	 * get saved (it happens outside of host_command_process), so we cannot
	 * verify the resent response itself.
	 */
}

ZTEST(host_cmd_host_commands, test_protocol)
{
	struct ec_response_test_protocol response;
	struct ec_params_test_protocol params = {
		.ec_result = EC_RES_SUCCESS,
		.ret_len = 4,
		.buf = { 1, 2, 3, 4 },
	};
	int rv;

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_TEST_PROTOCOL, 0, response, params);

	rv = host_command_process(&args);

	zassert_ok(rv, "Got %d", rv);
	zassert_mem_equal(params.buf, response.buf, params.ret_len);
}

ZTEST(host_cmd_host_commands, get_proto_version)
{
	struct ec_response_proto_version response;
	int rv;

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_RESPONSE(EC_CMD_PROTO_VERSION, 0, response);

	rv = host_command_process(&args);

	zassert_ok(rv, "Got %d", rv);
	zassert_equal(EC_PROTO_VERSION, response.version);
}

ZTEST_SUITE(host_cmd_host_commands, drivers_predicate_post_main, NULL, NULL,
	    NULL, NULL);
