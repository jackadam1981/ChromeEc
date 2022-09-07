/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h> /* nocheck */
#include <zephyr/ztest.h>

#include "console.h"
#include "ec_commands.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

ZTEST_USER(console_cmd_rtc, test_rtc_no_arg)
{
	const char *buffer;
	size_t buffer_size;
	int rv;

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_ok(rv, "Expected %d, returned %d", EC_SUCCESS, rv);
}

ZTEST_USER(console_cmd_rtc, test_rtc_invalid)
{
	const char *buffer;
	size_t buffer_size;
	int rv;

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc set");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_equal(rv, EC_ERROR_INVAL, "Expected %d, returned %d",
		      EC_ERROR_INVAL, rv);
}

ZTEST_USER(console_cmd_rtc, test_rtc_set)
{
	const char *buffer;
	size_t buffer_size;
	int rv;

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc set 5");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_ok(rv, "Expected %d, returned %d", EC_SUCCESS, rv);
}

ZTEST_USER(console_cmd_rtc, test_rtc_set_bad)
{
	const char *buffer;
	size_t buffer_size;
	int rv;

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc set t");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_equal(rv, EC_ERROR_PARAM2, "Expected %d, returned %d",
		      EC_ERROR_PARAM2, rv);
}

ZTEST_USER(console_cmd_rtc, test_rtc_alarm_no_args)
{
	const char *buffer;
	size_t buffer_size;
	int rv;

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc_alarm");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_ok(rv, "Expected %d, returned %d", EC_SUCCESS, rv);
	zassert_true(strcmp(buffer, "\r\nSetting RTC alarm\r\n") == 0,
		     "Invalid console output %s", buffer);
}

ZTEST_USER(console_cmd_rtc, test_rtc_alarm_good_args)
{
	const char *buffer;
	size_t buffer_size;
	int rv;

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc_alarm 1");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_ok(rv, "Expected %d, returned %d", EC_SUCCESS, rv);
	zassert_true(strcmp(buffer, "\r\nSetting RTC alarm\r\n") == 0,
		     "Invalid console output %s", buffer);

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc_alarm 1 5");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_ok(rv, "Expected %d, returned %d", EC_SUCCESS, rv);
	zassert_true(strcmp(buffer, "\r\nSetting RTC alarm\r\n") == 0,
		     "Invalid console output %s", buffer);
}

ZTEST_USER(console_cmd_rtc, test_rtc_alarm_bad_args)
{
	const char *buffer;
	size_t buffer_size;
	int rv;

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc_alarm t");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_equal(rv, EC_ERROR_PARAM1, "Expected %d, returned %d",
		      EC_ERROR_PARAM1, rv);

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc_alarm 1 t");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_equal(rv, EC_ERROR_PARAM2, "Expected %d, returned %d",
		      EC_ERROR_PARAM2, rv);
}

ZTEST_USER(console_cmd_rtc, test_rtc_alarm_reset)
{
	const char *buffer;
	size_t buffer_size;
	int rv;

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc_alarm 0");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_ok(rv, "Expected %d, returned %d", EC_SUCCESS, rv);
	zassert_true(strcmp(buffer, "\r\nSetting RTC alarm\r\n") == 0,
		     "Invalid console output %s", buffer);

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc_alarm 0 0");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_ok(rv, "Expected %d, returned %d", EC_SUCCESS, rv);
	zassert_true(strcmp(buffer, "\r\nSetting RTC alarm\r\n") == 0,
		     "Invalid console output %s", buffer);
}

ZTEST_SUITE(console_cmd_rtc, NULL, NULL, NULL, NULL, NULL);
