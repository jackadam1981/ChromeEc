/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef ZEPHYR_TEST_DRIVERS_INCLUDE_UTILS_H_
#define ZEPHYR_TEST_DRIVERS_INCLUDE_UTILS_H_

#if defined(CONFIG_SHELL_BACKEND_DUMMY)
/**
 * @brief Checks console command with expected console output and expected
 * return value
 *
 */
#define CHECK_CONSOLE_CMD(cmd, expected_output, expected_rv)                 \
	check_console_cmd((cmd), (expected_output), (expected_rv), __FILE__, \
			  __LINE__)
void check_console_cmd(const char *cmd, const char *expected_output,
		       const int expected_rv, const char *file, const int line);

/**
 * @brief Checks console command and reads it.
 *
 */
#define SCAN_CONSOLE_CMD(cmd, expected_rv, expected_count, format, ...)    \
	scan_console_cmd((cmd), (expected_rv), (expected_count), __FILE__, \
			 __LINE__, (format), __VA_ARGS__)
void scan_console_cmd(const char *cmd, const int expected_rv,
		      const int expected_count, const char *file,
		      const int line, const char *format, ...);
#endif /* CONFIG_SHELL_BACKEND_DUMMY */
#endif /* ZEPHYR_TEST_DRIVERS_INCLUDE_UTILS_H_ */
