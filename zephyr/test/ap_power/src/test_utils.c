/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "console.h"
#include "test_utils.h"

#if defined(CONFIG_SHELL_BACKEND_DUMMY)
#include <zephyr/shell/shell_dummy.h>
#endif
#include <zephyr/ztest.h>

#if defined(CONFIG_SHELL_BACKEND_DUMMY)
static void call_console_cmd(const char *cmd, const int expected_rv,
			     const char *file, const int line)
{
	int rv;

	shell_backend_dummy_clear_output(get_ec_shell());
	rv = shell_execute_cmd(get_ec_shell(), cmd);

	zassert_equal(expected_rv, rv,
		      "%s:%u \'%s\' - Expected %d, returned %d", file, line,
		      cmd, expected_rv, rv);
}

void check_console_cmd(const char *cmd, const char *expected_output,
		       const int expected_rv, const char *file, const int line)
{
	const char *buffer;
	size_t buffer_size;

	call_console_cmd(cmd, expected_rv, file, line);

	if (expected_output) {
		buffer = shell_backend_dummy_get_output(get_ec_shell(),
							&buffer_size);
		zassert_true(strstr(buffer, expected_output),
			     "Invalid console output %s", buffer);
	}
}

void scan_console_cmd(const char *cmd, const int expected_rv,
		      const int expected_count, const char *file,
		      const int line, const char *format, ...)
{
	const char *buffer;
	size_t buffer_size;
	va_list args;

	call_console_cmd(cmd, expected_rv, file, line);

	zassert_not_null(format);
	buffer = shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);
	va_start(args, format);
	int count = vsscanf(buffer, format, args);
	va_end(args);
	zassert_equal(expected_count, count,
		      "%s:%u \'%s\' outputs \'%s\' which does not match \'%s\'",
		      file, line, cmd, buffer, format);
}
#endif /* CONFIG_SHELL_BACKEND_DUMMY */
