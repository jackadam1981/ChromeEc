/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>

#include "console.h"
#include "ec_commands.h"
#include "system.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

ZTEST_USER(console_cmd_rtc, test_rtc_no_arg)
{
	char expected_buffer[32];
	uint32_t sec = 7;

	snprintf(expected_buffer, sizeof(expected_buffer),
		 "RTC: 0x%08x (%d.00 s)", sec, sec);

	system_set_rtc(sec);

	check_console_cmd("rtc", expected_buffer, EC_SUCCESS);
}

ZTEST_USER(console_cmd_rtc, test_rtc_invalid)
{
	check_console_cmd("rtc set", NULL, EC_ERROR_INVAL);
}

ZTEST_USER(console_cmd_rtc, test_rtc_set)
{
	char command[32];
	char expected_buffer[32];
	uint32_t sec = 48879;

	snprintf(expected_buffer, sizeof(expected_buffer),
		 "RTC: 0x%08x (%d.00 s)", sec, sec);
	snprintf(command, sizeof(command), "rtc set %d", sec);

	check_console_cmd(command, expected_buffer, EC_SUCCESS);
}

ZTEST_USER(console_cmd_rtc, test_rtc_set_bad)
{
	check_console_cmd("rtc set t", NULL, EC_ERROR_PARAM2);
}

ZTEST_USER(console_cmd_rtc, test_rtc_alarm_no_args)
{
	check_console_cmd("rtc_alarm", "Setting RTC alarm", EC_SUCCESS);
}

ZTEST_USER(console_cmd_rtc, test_rtc_alarm_good_args)
{
	check_console_cmd("rtc_alarm 1", "Setting RTC alarm", EC_SUCCESS);
	check_console_cmd("rtc_alarm 1 5", "Setting RTC alarm", EC_SUCCESS);
}

ZTEST_USER(console_cmd_rtc, test_rtc_alarm_bad_args)
{
	check_console_cmd("rtc_alarm t", NULL, EC_ERROR_PARAM1);
	check_console_cmd("rtc_alarm 1 t", NULL, EC_ERROR_PARAM2);
}

ZTEST_USER(console_cmd_rtc, test_rtc_alarm_reset)
{
	check_console_cmd("rtc_alarm 0", "Setting RTC alarm", EC_SUCCESS);
	check_console_cmd("rtc_alarm 0 0", "Setting RTC alarm", EC_SUCCESS);
}

ZTEST_SUITE(console_cmd_rtc, NULL, NULL, NULL, NULL, NULL);
