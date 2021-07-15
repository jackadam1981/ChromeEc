/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>

#include "ec_commands.h"
#include "host_command.h"

static void stub_send_response(struct host_cmd_handler_args *args) {}

static void test_host_command_get_protocol_info(void)
{
	struct host_cmd_handler_args args;
	struct ec_response_get_protocol_info response;

	args.send_response = stub_send_response;
	args.command = EC_CMD_GET_PROTOCOL_INFO;
	args.version = 1;
	args.response = &response;
	args.response_max = sizeof(struct ec_response_get_protocol_info);
	args.response_size = sizeof(struct ec_response_get_protocol_info);

	zassert_ok(host_command_process(&args), NULL);
	zassert_ok(args.result, NULL);
}

void test_suite_espi(void)
{
	ztest_test_suite(espi, ztest_user_unit_test(
				       test_host_command_get_protocol_info));
	ztest_run_test_suite(espi);
}
