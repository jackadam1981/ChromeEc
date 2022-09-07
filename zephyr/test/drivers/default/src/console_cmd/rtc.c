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
	char expected_buffer[32];
	uint32_t sec = 7;
	int rv;

	snprintf(expected_buffer, sizeof(expected_buffer),
		 "RTC: 0x%08x (%d.00 s)", sec, sec);

	system_set_rtc(sec);

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), "rtc");
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_ok(rv, "Expected %d, returned %d", EC_SUCCESS, rv);
	zassert_true(strstr(buffer, expected_buffer),
		     "Invalid console output %s", buffer);
}

ZTEST_USER(console_cmd_rtc, test_rtc_invalid)
{
	int rv;

	rv = shell_execute_cmd(get_ec_shell(), "rtc set");

	zassert_equal(rv, EC_ERROR_INVAL, "Expected %d, returned %d",
		      EC_ERROR_INVAL, rv);
}

ZTEST_USER(console_cmd_rtc, test_rtc_set)
{
	const char *buffer;
	size_t buffer_size;
	char command[32];
	char expected_buffer[32];
	uint32_t sec = 48879;
	int rv;

	snprintf(expected_buffer, sizeof(expected_buffer),
		 "RTC: 0x%08x (%d.00 s)", sec, sec);
	snprintf(command, sizeof(command), "rtc set %d", sec);

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), command);
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_ok(rv, "Expected %d, returned %d", EC_SUCCESS, rv);
	zassert_true(strstr(buffer, expected_buffer),
		     "Invalid console output %s", buffer);
}

ZTEST_USER(console_cmd_rtc, test_rtc_set_bad)
{
	int rv;

	rv = shell_execute_cmd(get_ec_shell(), "rtc set t");

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
	int rv;

	rv = shell_execute_cmd(get_ec_shell(), "rtc_alarm t");

	zassert_equal(rv, EC_ERROR_PARAM1, "Expected %d, returned %d",
		      EC_ERROR_PARAM1, rv);

	rv = shell_execute_cmd(get_ec_shell(), "rtc_alarm 1 t");

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
