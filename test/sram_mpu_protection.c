/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "system.h"
#include "task.h"
#include "test_util.h"

#include <stdint.h>
#include <string.h>

void hello_function(void)
{
	ccprints("Hello World!");
}

void bye_function(void)
{
	ccprints("Bye World!");
}

test_static int test_system_is_not_locked(void)
{
	TEST_EQ(system_is_locked(), 0, "%d");

	ccprints("Running hello_function.");
	hello_function();

	/* Copy No-op instruction. */
	uint16_t noop_instruction = 0xbf00;

	memcpy(&hello_function, &noop_instruction, 2);

	uint16_t instruction_copy = 0;

	memcpy(&instruction_copy, &hello_function, 2);
	TEST_EQ(instruction_copy, noop_instruction, "0x%04x");

	ccprints("Running hello_function again.");
	hello_function();

	return EC_SUCCESS;
}

test_static int change_is_locked(void)
{
	int res;
	char console_input[] = "flashwp true";

	res = test_send_console_command(console_input);
	TEST_EQ(res, EC_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static int clean_up(void)
{
	return EC_SUCCESS;
}

test_static int test_system_is_locked(void)
{
	TEST_EQ(system_is_locked(), 1, "%d");

	ccprints("Running bye_function.");
	bye_function();

	/* Copy No-op instruction. */
	uint16_t noop_instruction = 0xbf00;

	memcpy(&hello_function, &noop_instruction, 2);

	uint16_t instruction_copy = 0;

	memcpy(&instruction_copy, &hello_function, 2);
	TEST_EQ(instruction_copy, noop_instruction, "0x%04x");

	ccprints("Running bye_function again.");
	bye_function();

	return EC_SUCCESS;
}

test_static void run_test_step1(void)
{
	ccprints("Step 1: Run before System is Locked");
	cflush();

	RUN_TEST(test_system_is_not_locked);

	if (test_get_error_count()) {
		test_reboot_to_next_step(TEST_STATE_FAILED);
	} else {
		test_reboot_to_next_step(TEST_STATE_STEP_2);
	}
}

test_static void run_test_step2(void)
{
	ccprints("Step 2: Change System Lock Status");
	cflush();

	RUN_TEST(change_is_locked);

	if (test_get_error_count()) {
		test_reboot_to_next_step(TEST_STATE_FAILED);
	} else {
		test_reboot_to_next_step(TEST_STATE_STEP_3);
	}
}

test_static void run_test_step3(void)
{
	ccprints("Step 3: Run after System is locked");
	cflush();

	test_set_next_step(TEST_STATE_STEP_4);
	RUN_TEST(test_system_is_locked);

	if (test_get_error_count()) {
	} else {
		test_set_next_step(TEST_STATE_FAILED);
	}
}

test_static void run_test_step4(void)
{
	ccprints("Step 4: Cleanup");
	cflush();

	RUN_TEST(clean_up);

	if (test_get_error_count()) {
		test_reboot_to_next_step(TEST_STATE_FAILED);
	} else {
		test_reboot_to_next_step(TEST_STATE_PASSED);
	}
}

void test_run_step(uint32_t state)
{
	if (state & TEST_STATE_MASK(TEST_STATE_STEP_1)) {
		run_test_step1();
	} else if (state & TEST_STATE_MASK(TEST_STATE_STEP_2)) {
		run_test_step2();
	} else if (state & TEST_STATE_MASK(TEST_STATE_STEP_3)) {
		run_test_step3();
	} else if (state & TEST_STATE_MASK(TEST_STATE_STEP_4)) {
		run_test_step4();
	}
}
int task_test(void *unused)
{
	test_run_multistep();
	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();
	msleep(100); /* Wait for TASK_ID_TEST to initialize */
	task_wake(TASK_ID_TEST);
}
