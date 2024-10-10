/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "flash.h"
#include "string.h"
#include "system.h"
#include "task.h"
#include "test_util.h"

#include <stdint.h>
#include <string.h>

static bool write_protect_enabled;

void hello_function(void)
{
	ccprints("Hello World!");
}

void bye_function(void)
{
	ccprints("Bye World!");
}

test_static int test_ro_protection_enabled(void)
{
	TEST_BITS_SET(crec_flash_get_protect(), EC_FLASH_PROTECT_RO_NOW);

	return EC_SUCCESS;
}

static void print_usage(void)
{
	ccprintf("usage: runtest [wp_on|wp_off]\n");
}

test_static int test_system_is_not_locked(void)
{
	TEST_EQ(system_is_locked(), 0, "%d");

	ccprints("Running hello_function.");
	cflush();
	hello_function();

	/* Copy No-op instruction. */
	uint16_t noop_instruction = 0xbf00;
	memcpy(&hello_function, &noop_instruction, 2);

	uint16_t instruction_copy = 0;
	memcpy(&instruction_copy, &hello_function, 2);
	TEST_EQ(instruction_copy, noop_instruction, "0x%04x");

	return EC_SUCCESS;
}

test_static int test_system_is_locked(void)
{
	TEST_EQ(system_is_locked(), 1, "%d");

	ccprints("Running bye_function.");
	cflush();
	bye_function();

	/* Copy No-op instruction. */
	uint16_t noop_instruction = 0xbf00;
	memcpy(&hello_function, &noop_instruction, 2);

	uint16_t instruction_copy = 0;
	/* This should cause a reboot. */
	memcpy(&instruction_copy, &hello_function, 2);
	TEST_EQ(instruction_copy, noop_instruction, "0x%04x");

	return EC_SUCCESS;
}

test_static int test_system_status(void)
{
	char console_input[] = "sysinfo";

	enum ec_error_list res = test_send_console_command(console_input);
	TEST_EQ(res, EC_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static void test_run_step1(void)
{
	ccprints("Step 1: Running system is locked %s",
		 write_protect_enabled ? "on" : "off");
	cflush();

	test_set_next_step(TEST_STATE_STEP_2);
	if (write_protect_enabled) {
		RUN_TEST(test_system_is_locked);
		if (test_get_error_count()) {
		} else {
			test_set_next_step(TEST_STATE_FAILED);
		}
	} else {
		RUN_TEST(test_system_is_not_locked);
		if (test_get_error_count()) {
			test_reboot_to_next_step(TEST_STATE_FAILED);
		} else {
			test_reboot_to_next_step(TEST_STATE_STEP_2);
		}
	}
}

test_static void run_test_step2(void)
{
	ccprints("Step 2: Getting system status.");
	cflush();

	RUN_TEST(test_system_status);
	if (test_get_error_count()) {
		test_reboot_to_next_step(TEST_STATE_FAILED);
	} else {
		test_reboot_to_next_step(TEST_STATE_PASSED);
	}
}

void test_run_step(uint32_t state)
{
	if (state & TEST_STATE_MASK(TEST_STATE_STEP_1)) {
		test_run_step1();
	} else if (state & TEST_STATE_MASK(TEST_STATE_STEP_2)) {
		run_test_step2();
	}
}

int task_test(void *unused)
{
	if (IS_ENABLED(SECTION_IS_RW)) {
		test_run_multistep();
	}
	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();

	if (IS_ENABLED(CONFIG_SYSTEM_UNLOCKED)) {
		ccprintf("Please disable CONFIG_SYSTEM_UNLOCKED before "
			 "running this test\n");
		test_fail();
		return;
	}

	if (argc < 2) {
		print_usage();
		test_fail();
		return;
	}

	if (strncmp(argv[1], "wp_on", 5) == 0)
		write_protect_enabled = true;
	else if (strncmp(argv[1], "wp_off", 6) == 0) {
		write_protect_enabled = false;
		if (IS_ENABLED(CONFIG_WP_ALWAYS)) {
			ccprintf("Hardware write protect always enabled. "
				 "Please disable CONFIG_WP_ALWAYS before "
				 "running this test\n");
			test_fail();
			return;
		}
	} else {
		print_usage();
		test_fail();
		return;
	}

	crec_msleep(30); /* Wait for TASK_ID_TEST to initialize */
	task_wake(TASK_ID_TEST);
}
