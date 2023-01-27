/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "panic.h"
#include "system.h"
#include "task.h"
#include "test_util.h"

#ifdef SECTION_IS_RW
#include "fpsensor_state.h"
#endif

#include <stdint.h>
#include <string.h>

#ifdef SECTION_IS_RW
static const uint8_t default_fake_tpm_seed[] = {
	0xd9, 0x71, 0xaf, 0xc4, 0xcd, 0x36, 0xe3, 0x60, 0xf8, 0x5a, 0xa0,
	0xa6, 0x2c, 0xb3, 0xf5, 0xe2, 0xeb, 0xb9, 0xd8, 0x2f, 0xb5, 0x78,
	0x5c, 0x79, 0x82, 0xce, 0x06, 0x3f, 0xcc, 0x23, 0xb9, 0xe7,
};

static const uint8_t zero_fake_tpm_seed[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
#endif

test_static int test_tpm_seed_before_reboot(void)
{
#ifdef SECTION_IS_RW

	TEST_ASSERT_ARRAY_EQ(tpm_seed, zero_fake_tpm_seed, sizeof(tpm_seed));

	memcpy(tpm_seed, default_fake_tpm_seed, FP_CONTEXT_TPM_BYTES);

	TEST_ASSERT_ARRAY_EQ(tpm_seed, default_fake_tpm_seed, sizeof(tpm_seed));

#endif

	return EC_SUCCESS;
}

test_static int test_tpm_seed_after_reboot(void)
{
#ifdef SECTION_IS_RW

	TEST_ASSERT_ARRAY_EQ(tpm_seed, zero_fake_tpm_seed, sizeof(tpm_seed));

#endif

	return EC_SUCCESS;
}

test_static int test_panic_data(void)
{
	const uint32_t expected_reason = 0;
	const uint32_t expected_info = 0;
	/*
	 * https://developer.arm.com/documentation/dui0552/a/the-cortex-m3-processor/exception-model/exception-types
	 */
	const uint8_t expected_exception = 6; /* usage fault */

	uint32_t reason = UINT32_MAX;
	uint32_t info = UINT32_MAX;
	uint8_t exception = UINT8_MAX;

	panic_get_reason(&reason, &info, &exception);

	TEST_EQ(reason, expected_reason, "%08x");
	TEST_EQ(info, expected_info, "%d");
	TEST_EQ(exception, expected_exception, "%d");

	return EC_SUCCESS;
}

test_static void run_test_step1(void)
{
	ccprints("Step 1: tpm_seed_clear\n");
	cflush();
	test_set_next_step(TEST_STATE_STEP_2);
	RUN_TEST(test_tpm_seed_before_reboot);
}

test_static void run_test_step2(void)
{
	ccprints("Step 2: tpm_seed_clear\n");
	cflush();
	RUN_TEST(test_panic_data);

	if (test_get_error_count())
		test_reboot_to_next_step(TEST_STATE_FAILED);
	else
		test_reboot_to_next_step(TEST_STATE_STEP_3);
}

test_static void run_test_step3(void)
{
	ccprints("Step 3: tpm_seed_clear\n");
	cflush();
	RUN_TEST(test_tpm_seed_after_reboot);

	RUN_TEST(test_panic_data);

	if (test_get_error_count())
		test_reboot_to_next_step(TEST_STATE_FAILED);
	else
		test_reboot_to_next_step(TEST_STATE_PASSED);
}

void test_run_step(uint32_t state)
{
	if (state & TEST_STATE_MASK(TEST_STATE_STEP_1)) {
		run_test_step1();
	} else if (state & TEST_STATE_MASK(TEST_STATE_STEP_2)) {
		run_test_step2();
	} else if (state & TEST_STATE_MASK(TEST_STATE_STEP_3)) {
		run_test_step3();
	}
}

int task_test(void *unused)
{
	if (IS_ENABLED(SECTION_IS_RW))
		test_run_multistep();
	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();
	msleep(30); /* Wait for TASK_ID_TEST to initialize */
	task_wake(TASK_ID_TEST);
}
