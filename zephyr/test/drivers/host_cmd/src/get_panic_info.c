/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"
#include "panic.h"
#include "test/drivers/test_state.h"

#include <zephyr/ztest.h>

struct host_cmd_get_panic_info_fixture {
	struct panic_data saved_pdata;
};

static void *host_cmd_get_panic_info_setup(void)
{
	static struct host_cmd_get_panic_info_fixture fixture = { 0 };

	return &fixture;
}

static void host_cmd_get_panic_info_before(void *f)
{
	struct host_cmd_get_panic_info_fixture *fixture = f;
	struct panic_data *pdata = get_panic_data_write();

	fixture->saved_pdata = *pdata;
}

static void host_cmd_get_panic_info_after(void *f)
{
	struct host_cmd_get_panic_info_fixture *fixture = f;
	struct panic_data *pdata = get_panic_data_write();

	*pdata = fixture->saved_pdata;
}

ZTEST_SUITE(host_cmd_get_panic_info, drivers_predicate_post_main,
	    host_cmd_get_panic_info_setup, host_cmd_get_panic_info_before,
	    host_cmd_get_panic_info_after, NULL);

ZTEST_USER(host_cmd_get_panic_info, test_get_panic_info)
{
	struct panic_data *pdata = get_panic_data_write();
	struct panic_data response = { 0 };
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_GET_PANIC_INFO, UINT8_C(0), response);

	pdata->arch = 0;
	pdata->struct_version = 1;
	pdata->flags = 2;
	pdata->reserved = 3;
	pdata->struct_size = sizeof(struct panic_data);
	pdata->magic = PANIC_DATA_MAGIC;

	zassert_ok(host_command_process(&args), NULL);
	zassert_equal(sizeof(struct panic_data), args.response_size, NULL);
	zassert_equal(0, response.arch, NULL);
	zassert_equal(1, response.struct_version, NULL);
	zassert_equal(2, response.flags, NULL);
	zassert_equal(3, response.reserved, NULL);
	zassert_equal(sizeof(struct panic_data), response.struct_size, NULL);
	zassert_equal(PANIC_DATA_MAGIC, response.magic, NULL);
	zassert_equal(pdata->flags & PANIC_DATA_FLAG_OLD_HOSTCMD,
		      PANIC_DATA_FLAG_OLD_HOSTCMD, NULL);
}

ZTEST_USER(host_cmd_get_panic_info, test_get_panic_info_bad_magic)
{
	struct panic_data *pdata = get_panic_data_write();
	struct panic_data expected = { 0 };
	struct panic_data response = { 0 };
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_GET_PANIC_INFO, UINT8_C(0), response);

	pdata->magic = PANIC_DATA_MAGIC + 1;
	zassert_ok(host_command_process(&args), NULL);
	/* Check that nothing was written to response */
	zassert_mem_equal(&response, &expected, sizeof(struct panic_data),
			  NULL);
}

ZTEST_USER(host_cmd_get_panic_info, test_get_panic_info_size_is_zero)
{
	struct panic_data *pdata = get_panic_data_write();
	struct panic_data expected = { 0 };
	struct panic_data response = { 0 };
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_GET_PANIC_INFO, UINT8_C(0), response);

	pdata->magic = PANIC_DATA_MAGIC;
	pdata->struct_size = 0;
	zassert_ok(host_command_process(&args), NULL);
	/* Check that nothing was written to response */
	zassert_mem_equal(&response, &expected, sizeof(struct panic_data),
			  NULL);
}

ZTEST_USER(host_cmd_get_panic_info, test_get_panic_info_truncate)
{
	struct panic_data *pdata = get_panic_data_write();
	struct panic_data response = { 0 };
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_GET_PANIC_INFO, UINT8_C(0), response);

	/* Panic data is bigger than max response size, should be truncated */
	args.response_max = sizeof(struct panic_data) - 2;

	pdata->flags = 0;
	pdata->magic = PANIC_DATA_MAGIC;
	pdata->struct_size = sizeof(struct panic_data);

	zassert_ok(host_command_process(&args), NULL);
	zassert_equal(sizeof(struct panic_data) - 2, args.response_size, NULL);
	/* Truncation flag must be set */
	zassert_equal(response.flags & PANIC_DATA_FLAG_TRUNCATED,
		      PANIC_DATA_FLAG_TRUNCATED, NULL);
	zassert_equal(sizeof(struct panic_data), response.struct_size, NULL);
}

ZTEST_USER(host_cmd_get_panic_info, test_get_panic_flags_unavailable)
{
	struct panic_data *pdata = get_panic_data_write();
	uint8_t flags;
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_GET_PANIC_FLAGS, UINT8_C(0), flags);

	pdata->magic = 0;
	pdata->struct_size = sizeof(struct panic_data);

	zassert_equal(host_command_process(&args), EC_RES_UNAVAILABLE);
}

ZTEST_USER(host_cmd_get_panic_info, test_get_panic_flags_too_big)
{
	struct panic_data *pdata = get_panic_data_write();
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_SIMPLE(EC_CMD_GET_PANIC_FLAGS, UINT8_C(0));

	pdata->magic = PANIC_DATA_MAGIC;
	pdata->struct_size = sizeof(struct panic_data);

	zassert_equal(host_command_process(&args), EC_RES_RESPONSE_TOO_BIG);
}

ZTEST_USER(host_cmd_get_panic_info, test_get_panic_flags)
{
	struct panic_data *pdata = get_panic_data_write();
	uint8_t flags;
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_GET_PANIC_FLAGS, UINT8_C(0), flags);

	pdata->flags = 1;
	pdata->magic = PANIC_DATA_MAGIC;
	pdata->struct_size = sizeof(struct panic_data);

	zassert_ok(host_command_process(&args));
	zassert_equal(flags, 1);
	zassert_equal(pdata->flags, 1);
	/* EC_CMD_GET_PANIC_FLAGS should not set PANIC_DATA_FLAG_OLD_HOSTCMD */
	zassert_equal(pdata->flags & PANIC_DATA_FLAG_OLD_HOSTCMD, 0, NULL);
}
