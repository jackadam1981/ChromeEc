/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpc_private.h"
#include "fpsensor_driver.h"
#include "test_util.h"

#include <stdio.h>

static int is_locked;

int system_is_locked(void)
{
	return is_locked;
}

test_static int test_command_mem_dump(void)
{
	enum ec_error_list res;
	/* This word will be read by the md command. */
	const volatile uint32_t valid_word = 0x1badd00d;
	/* Compose the md console command to read |valid_word|. */
	char console_input[] = "md 0x01234567";

	snprintf(console_input, sizeof(console_input), "md %p", &valid_word);

	is_locked = 0;
	res = test_send_console_command(console_input);
	TEST_EQ(res, EC_SUCCESS, "%d");

	is_locked = 1;
	res = test_send_console_command(console_input);
	TEST_EQ(res, EC_ERROR_ACCESS_DENIED, "%d");

	return EC_SUCCESS;
}

test_static int test_command_read_write_word(void)
{
	enum ec_error_list res;
	const uint32_t old_value = 0x1badd00d;
	/* This word will be read/written by the rw command. */
	volatile uint32_t valid_word = old_value;
	/* Compose the rw console command to write |valid_word| with a value
	 * of 5.
	 */
	char console_input[] = "rw 0x01234567 0x05";
	const uint32_t new_value = 0x05;

	snprintf(console_input, sizeof(console_input), "rw %p 0x%02x",
		 &valid_word, new_value);

	is_locked = 0;
	res = test_send_console_command(console_input);
	TEST_EQ(res, EC_SUCCESS, "%d");
	TEST_EQ(new_value, valid_word, "%" PRIu32);

	is_locked = 1;
	/* Reset valid word */
	valid_word = old_value;
	res = test_send_console_command(console_input);
	TEST_EQ(res, EC_ERROR_ACCESS_DENIED, "%d");
	TEST_EQ(old_value, valid_word, "%" PRIu32);

	return EC_SUCCESS;
}

test_static int test_command_fpupload(void)
{
	enum ec_error_list res;

	/* System is unlocked. */
	is_locked = 0;

	// Test for case when number of arguments is not equals to 3.
	char console_input1[] = "fpupload 52 image long";
	res = test_send_console_command(console_input1);
	TEST_EQ(res, EC_ERROR_PARAM1, "%d");

	// Test for the case when offset < 0.
	char console_input2[] = "fpupload -1 image";
	res = test_send_console_command(console_input2);
	TEST_EQ(res, EC_ERROR_PARAM2, "%d");

#if defined(CONFIG_FP_SENSOR_FPC1025) || defined(CONFIG_FP_SENSOR_FPC1145) || \
	defined(CONFIG_FP_SENSOR_ELAN80SG)
	// Test for the case when dest >= fp_buffer + FP_SENSOR_IMAGE_SIZE.
	char console_input3[] = "fpupload -1 image";
	const char *pixels_str = "image";

	int offset = FP_SENSOR_IMAGE_SIZE - FP_SENSOR_IMAGE_OFFSET;
	snprintf(console_input3, sizeof(console_input3), "fpupload %d %s",
		 offset, pixels_str);
	res = test_send_console_command(console_input3);
	TEST_EQ(res, EC_ERROR_PARAM3, "%d");
#endif

	// Test for the success case.
	char console_input4[] = "fpupload 52 image";
	res = test_send_console_command(console_input4);
	TEST_EQ(res, EC_SUCCESS, "%d");

	/* System is locked. */
	is_locked = 1;

	// Test for the case when access is denied.
	char console_input5[] = "fpupload 52 image";
	res = test_send_console_command(console_input5);
	TEST_EQ(res, EC_ERROR_ACCESS_DENIED, "%d");

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();

	RUN_TEST(test_command_mem_dump);
	RUN_TEST(test_command_read_write_word);
	RUN_TEST(test_command_fpupload);

	test_print_result();
}
