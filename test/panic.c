/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "assert.h"
#include "panic.h"
#include "system.h"
#include "task.h"
#include "test_util.h"

#if !(defined(CORE_CORTEX_M) || defined(CORE_CORTEX_M0))
#error "Architecture not supported"
#endif

struct reg_vals {
	int index;
	uint32_t val;
};

static const struct reg_vals EXPECTED[] = {
	{ .index = CORTEX_PANIC_REGISTER_R4, .val = 4 },
	{ .index = CORTEX_PANIC_REGISTER_R5, .val = 5 },
	{ .index = CORTEX_PANIC_REGISTER_R6, .val = 6 },
	{ .index = CORTEX_PANIC_REGISTER_R7, .val = 7 },
	{ .index = CORTEX_PANIC_REGISTER_R8, .val = 8 },
	{ .index = CORTEX_PANIC_REGISTER_R9, .val = 9 },
	{ .index = CORTEX_PANIC_REGISTER_R10, .val = 10 },
	{ .index = CORTEX_PANIC_REGISTER_R11, .val = 11 },
	/*
	 * On Cortex-M we can use "b", but on Cortex-M0 we have to use "bl",
	 * which clobbers the link register (R14).
	 */
#ifdef CORE_CORTEX_M
	{ .index = CORTEX_PANIC_REGISTER_LR, .val = 14 },
#endif
};

test_static int test_exception_panic_registers(void)
{
	if (IS_ENABLED(CORE_CORTEX_M)) {
		asm volatile("movs r0, #0\n"
			     "movs r1, #1\n"
			     "movs r2, #2\n"
			     "movs r3, #3\n"
			     "movs r4, #4\n"
			     "movs r5, #5\n"
			     "movs r6, #6\n"
			     "movs r7, #7\n"
			     "movs r8, #8\n"
			     "movs r9, #9\n"
			     "movs r10, #10\n"
			     "movs r11, #11\n"
			     "movs r14, #14\n"
			     /*
			      * Using branch (b) instead of branch with link
			      * (bl) so that we preserve the link register
			      * (r14).
			      */
			     "b exception_panic\n");
	} else if (IS_ENABLED(CORE_CORTEX_M0)) {
		asm volatile("movs r1, #1\n"
			     "movs r2, #2\n"
			     "movs r3, #3\n"
			     "movs r4, #4\n"
			     "movs r5, #5\n"
			     "movs r6, #6\n"
			     "movs r7, #7\n"
			     "movs r0, #8\n"
			     "mov r8, r0\n"
			     "movs r0, #9\n"
			     "mov r9, r0\n"
			     "movs r0, #10\n"
			     "mov r10, r0\n"
			     "movs r0, #11\n"
			     "mov r11, r0\n"
			     "movs r0, #14\n"
			     "mov r14, r0\n"
			     "movs r0, #0\n"
			     /*
			      * Using branch (b) doesn't compile, so have to
			      * use branch with link (bl).
			      */
			     "bl exception_panic\n");
	}
	__builtin_unreachable();
}

test_static void run_test_step1(void)
{
	ccprintf("Step 1: Panic\n");
	system_set_scratchpad(TEST_STATE_MASK(TEST_STATE_STEP_2));
	RUN_TEST(test_exception_panic_registers);
}

test_static int run_test_step2(void)
{
	struct panic_data *data;
	int i;

	ccprintf("Step 2: Read panic data\n");
	data = panic_get_data();
	for (i = 0; i < ARRAY_SIZE(EXPECTED); i++) {
		TEST_EQ(EXPECTED[i].val, data->cm.regs[EXPECTED[i].index],
			"%d");
		cflush();
	}
	return EC_SUCCESS;
}

void test_run_step(uint32_t state)
{
	int ret;

	if (state & TEST_STATE_MASK(TEST_STATE_STEP_1))
		run_test_step1();
	else if (state & TEST_STATE_MASK(TEST_STATE_STEP_2)) {
		ret = run_test_step2();
		if (IS_ENABLED(CORE_CORTEX_M)) {
			if (ret == EC_SUCCESS)
				test_reboot_to_next_step(TEST_STATE_PASSED);
			else
				test_reboot_to_next_step(TEST_STATE_FAILED);
		} else if (IS_ENABLED(CORE_CORTEX_M0)) {
			/*
			 * Avoiding the reboot that happens above. The test
			 * will pass using the above, but it seems that panic
			 * info displayed with "panicinfo" is no longer valid.
			 */
			test_clean_up();
			system_set_scratchpad(0);
			if (ret == EC_SUCCESS)
				test_pass();
			else
				test_fail();

		}
	}
}

int task_test(void *unused)
{
	test_run_multistep();
	return EC_SUCCESS;
}

void run_test(int argc, char **argv)
{
	msleep(30); /* Wait for TASK_ID_TEST to initialize */
	task_wake(TASK_ID_TEST);
}
