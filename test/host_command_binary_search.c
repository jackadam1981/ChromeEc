/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests host command lookup that uses binary search
 */

#include "host_command.h"
#include "test_util.h"
#include "util.h"

#define CMD_VERSION 0
#define CMD_MASK 1

#define CMD0	0
#define CMD1	1
#define CMD2	2
#define CMD3	3
#define CMD4	4
#define CMD5	5
#define CMD6	6
#define CMD7	7
#define CMD8	8
#define CMD9	9
#define CMD10   10
#define CMD11	11
#define CMD12	12

/* NO COMMAND 0 HANDLER */
static int handler1(struct host_cmd_handler_args *args);
static int handler2(struct host_cmd_handler_args *args);
static int handler3(struct host_cmd_handler_args *args);
static int handler4(struct host_cmd_handler_args *args);
static int handler5(struct host_cmd_handler_args *args);
/* NO COMMAND 6 HANDLER */
static int handler7(struct host_cmd_handler_args *args);
static int handler8(struct host_cmd_handler_args *args);
static int handler9(struct host_cmd_handler_args *args);
static int handler10(struct host_cmd_handler_args *args);
static int handler11(struct host_cmd_handler_args *args);
/* NO COMMAND 12 HANDLER */

static struct __attribute__((__packed__)) host_command test_commands[10] = {
	{handler1, CMD1, CMD_MASK},
	{handler2, CMD2, CMD_MASK},
	{handler3, CMD3, CMD_MASK},
	{handler4, CMD4, CMD_MASK},
	{handler5, CMD5, CMD_MASK},
	{handler7, CMD7, CMD_MASK},
	{handler8, CMD8, CMD_MASK},
	{handler9, CMD9, CMD_MASK},
	{handler10, CMD10, CMD_MASK},
	{handler11, CMD11, CMD_MASK}
};

struct host_command *__hcmds;
struct host_command *__hcmds_end;

static int handler1(struct host_cmd_handler_args *args)
{
	return EC_RES_SUCCESS;
}

static int handler2(struct host_cmd_handler_args *args)
{
	return EC_RES_SUCCESS;
}

static int handler3(struct host_cmd_handler_args *args)
{
	return EC_RES_SUCCESS;
}

static int handler4(struct host_cmd_handler_args *args)
{
	return EC_RES_SUCCESS;
}

static int handler5(struct host_cmd_handler_args *args)
{
	return EC_RES_SUCCESS;
}

static int handler7(struct host_cmd_handler_args *args)
{
	return EC_RES_SUCCESS;
}

static int handler8(struct host_cmd_handler_args *args)
{
	return EC_RES_SUCCESS;
}

static int handler9(struct host_cmd_handler_args *args)
{
	return EC_RES_SUCCESS;
}

static int handler10(struct host_cmd_handler_args *args)
{
	return EC_RES_SUCCESS;
}

static int handler11(struct host_cmd_handler_args *args)
{
	return EC_RES_SUCCESS;
}

/*****************************************************************************/
/* Tests */
static int test_zero_cmds(void)
{
	struct host_cmd_handler_args args;
	struct ec_response_get_next_event event;

	__hcmds = test_commands;
	__hcmds_end = __hcmds;

	args.version = CMD_VERSION;
	args.params = NULL;
	args.params_size = 0;
	args.response = &event;
	args.response_max = sizeof(event);
	args.response_size = 0;

	args.command = CMD1;

	TEST_ASSERT(host_command_process(&args) == EC_RES_INVALID_COMMAND);

	return EC_SUCCESS;
}

static int test_one_cmds(void)
{
	struct host_cmd_handler_args args;
	struct ec_response_get_next_event event;

	__hcmds = test_commands;
	__hcmds_end = &test_commands[1];

	args.version = CMD_VERSION;
	args.params = NULL;
	args.params_size = 0;
	args.response = &event;
	args.response_max = sizeof(event);
	args.response_size = 0;

	args.command = CMD1;
	TEST_ASSERT(host_command_process(&args) == EC_RES_SUCCESS);

	return EC_SUCCESS;
}

static int test_two_cmds(void)
{
	struct host_cmd_handler_args args;
	struct ec_response_get_next_event event;

	__hcmds = test_commands;
	__hcmds_end = &test_commands[2];

	args.version = CMD_VERSION;
	args.params = NULL;
	args.params_size = 0;
	args.response = &event;
	args.response_max = sizeof(event);
	args.response_size = 0;

	args.command = CMD1;
	TEST_ASSERT(host_command_process(&args) == EC_RES_SUCCESS);

	args.command = CMD2;
	TEST_ASSERT(host_command_process(&args) == EC_RES_SUCCESS);

	return EC_SUCCESS;
}

static int test_all_cmds(void)
{
	struct host_cmd_handler_args args;
	struct ec_response_get_next_event event;
	int i;

	__hcmds = test_commands;
	__hcmds_end = &test_commands[10];

	args.version = CMD_VERSION;
	args.params = NULL;
	args.params_size = 0;
	args.response = &event;
	args.response_max = sizeof(event);
	args.response_size = 0;

	for (i = 1; i <= CMD5; i++) {
		args.command = i;
		TEST_ASSERT(host_command_process(&args) == EC_RES_SUCCESS);
	}

	for (i = 7; i <= CMD11; i++) {
		args.command = i;
		TEST_ASSERT(host_command_process(&args) == EC_RES_SUCCESS);
	}

	return EC_SUCCESS;
}


static int test_cmd_not_found(void)
{
	struct host_cmd_handler_args args;
	struct ec_response_get_next_event event;

	__hcmds = test_commands;
	__hcmds_end = &test_commands[10];

	args.version = CMD_VERSION;
	args.params = NULL;
	args.params_size = 0;
	args.response = &event;
	args.response_max = sizeof(event);
	args.response_size = 0;

	args.command = CMD0;
	TEST_ASSERT(host_command_process(&args) == EC_RES_INVALID_COMMAND);

	args.command = CMD6;
	TEST_ASSERT(host_command_process(&args) == EC_RES_INVALID_COMMAND);

	args.command = CMD12;
	TEST_ASSERT(host_command_process(&args) == EC_RES_INVALID_COMMAND);

	return EC_SUCCESS;
}

static int test_hcmds_end_lt_hcmds(void)
{
	struct host_cmd_handler_args args;
	struct ec_response_get_next_event event;
	int i;

	__hcmds_end = test_commands;
	__hcmds = &test_commands[10];

	args.version = CMD_VERSION;
	args.params = NULL;
	args.params_size = 0;
	args.response = &event;
	args.response_max = sizeof(event);
	args.response_size = 0;

	for (i = 0; i < 12; i++) {
		args.command = i;
		TEST_ASSERT(host_command_process(&args)
			== EC_RES_INVALID_COMMAND);
	}

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_zero_cmds);
	RUN_TEST(test_one_cmds);
	RUN_TEST(test_two_cmds);
	RUN_TEST(test_all_cmds);
	RUN_TEST(test_cmd_not_found);
	RUN_TEST(test_hcmds_end_lt_hcmds);

	test_print_result();
}
