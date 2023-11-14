/* Copyright 2013 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test utilities.
 */

#if defined(TEST_COVERAGE) || defined(TEST_HOSTTEST)
/* We need signal() and exit() only when building to run on the host. */
#include <signal.h>
#include <stdlib.h>
#endif

#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "system.h"
#include "task.h"
#include "test_util.h"
#include "util.h"

#include <assert.h>

struct test_util_tag {
	uint8_t error_count;
};

#define TEST_UTIL_SYSJUMP_TAG 0x5455 /* "TU" */
#define TEST_UTIL_SYSJUMP_VERSION 1

int __test_error_count;

/* Weak reference function as an entry point for unit test */
test_mockable void run_test(int argc, const char **argv)
{
	/* This function should always be replaced by a real implementation of
	 * run_test().
	 * If this assertion is failing, it means your test was not linked
	 * correctly. Check the signature of run_test() defined in your test
	 * matches the one above.
	 */
	ccprintf("%s:%d: ASSERTION failed: ran weakly linked fallback test\n",
		 __FILE__, __LINE__);
	assert(0);
}

/* Default mock test init */
test_mockable void test_init(void)
{
}

/* Default mock before test */
test_mockable void before_test(void)
{
}

/* Default mock after test */
test_mockable void after_test(void)
{
}

#ifdef TEST_COVERAGE
extern void __gcov_dump(void);
extern void __gcov_reset(void);

void emulator_flush(void)
{
	__gcov_dump();
	__gcov_reset();
}
#else
void emulator_flush(void)
{
}
#endif

#if defined(TEST_HOSTTEST) || defined(TEST_COVERAGE)
/* Host-based unit tests need to exit(0) when they receive a SIGTERM. */
void test_end_hook(int sig)
{
	emulator_flush();
	exit(0);
}

void register_test_end_hook(void)
{
	signal(SIGTERM, test_end_hook);
}
#else
void register_test_end_hook(void)
{
}
#endif

void test_reset(void)
{
	if (!system_jumped_to_this_image())
		__test_error_count = 0;
}

void test_pass(void)
{
	ccprintf("Pass!\n");
}

void test_fail(void)
{
	ccprintf("Fail!\n");
}

void test_print_result(void)
{
	if (__test_error_count)
		ccprintf("Fail! (%d tests)\n", __test_error_count);
	else
		ccprintf("Pass!\n");
}

int test_get_error_count(void)
{
	return __test_error_count;
}

static struct test_multistep_state test_multistep_get_state(void)
{
	uint32_t state;
	uint16_t state_16bit;

	system_get_scratchpad(&state);
	state_16bit = (uint16_t)state;

	return *(struct test_multistep_state *)(&state_16bit);
}

static void test_multistep_set_state(struct test_multistep_state state)
{
	uint16_t state_16bit;

	*(struct test_multistep_state *)(&state_16bit) = state;
	system_set_scratchpad((uint32_t)state_16bit);
}

static void test_multistep_reset_state(void)
{
	test_multistep_set_state((struct test_multistep_state){ 0 });
}

static void test_multistep_reboot_with_state(struct test_multistep_state state)
{
	test_multistep_set_state(state);
	system_reset(SYSTEM_RESET_HARD);
}

enum test_multistep_status test_multistep_get_status(void)
{
	return test_multistep_get_state().status;
}

enum test_multistep_step test_multistep_get_step(void)
{
	return test_multistep_get_state().step;
}

void test_multistep_set_step(enum test_multistep_step step)
{
	test_multistep_set_state((struct test_multistep_state){
		.status = TEST_MULTISTEP_STATUS_DEFAULT,
		.step = step,
	});
}

void test_multistep_reboot_to_next_step(enum test_multistep_step step)
{
	ccprintf("# TEST MULTISTEP: Rebooting to step %d (err count = %d).\n",
		 step + 1, test_get_error_count());
	cflush();
	test_multistep_reboot_with_state((struct test_multistep_state){
		.status = TEST_MULTISTEP_STATUS_DEFAULT,
		.step = step,
	});
}

void test_multistep_finish_with_status(enum test_multistep_status status)
{
	if (status == TEST_MULTISTEP_STATUS_DEFAULT) {
		status = test_get_error_count() == 0 ?
				 TEST_MULTISTEP_STATUS_PASSED :
				 TEST_MULTISTEP_STATUS_FAILED;
	}
	ccprintf("# TEST MULTISTEP: Finishing with status %s.\n",
		 (status == TEST_MULTISTEP_STATUS_PASSED) ? "PASSED" :
							    "FAILED");
	cflush();
	test_multistep_reboot_with_state((struct test_multistep_state){
		.status = status,
		.step = TEST_MULTISTEP_STEP_1,
	});
}

__overridable void test_multistep_clean_up(void)
{
}

__overridable void test_multistep_run_step(enum test_multistep_step step)
{
	ccprintf("# TEST MULTISTEP: Ran unimplemented %s.\n", __func__);
	cflush();
}

void test_run_multistep(void)
{
	struct test_multistep_state state = test_multistep_get_state();

	if (state.status == TEST_MULTISTEP_STATUS_PASSED) {
		test_multistep_clean_up();
		test_multistep_reset_state();
		test_pass();
		return;
	} else if (state.status == TEST_MULTISTEP_STATUS_FAILED) {
		test_multistep_clean_up();
		test_multistep_reset_state();
		test_fail();
		return;
	}

	if (state.step == TEST_MULTISTEP_STEP_1)
		task_wait_event(-1); /* Wait for run_test() */

	ccprintf("# TEST MULTISTEP: Running step %d.\n", state.step + 1);
	cflush();
	test_multistep_run_step(state.step);
}

#ifdef HAS_TASK_HOSTCMD
enum ec_status test_send_host_command(int command, int version,
				      const void *params, int params_size,
				      void *resp, int resp_size)
{
	struct host_cmd_handler_args args;

	args.version = version;
	args.command = command;
	args.params = params;
	args.params_size = params_size;
	args.response = resp;
	args.response_max = resp_size;
	args.response_size = 0;

	return host_command_process(&args);
}
#endif /* TASK_HAS_HOSTCMD */

/* Linear congruential pseudo random number generator */
uint32_t prng(uint32_t seed)
{
	return 22695477 * seed + 1;
}

uint32_t prng_no_seed(void)
{
	static uint32_t seed = 0x1234abcd;
	return seed = prng(seed);
}

static void restore_state(void)
{
	const struct test_util_tag *tag;
	int version, size;

	tag = (const struct test_util_tag *)system_get_jump_tag(
		TEST_UTIL_SYSJUMP_TAG, &version, &size);
	if (tag && version == TEST_UTIL_SYSJUMP_VERSION && size == sizeof(*tag))
		__test_error_count = tag->error_count;
	else
		__test_error_count = 0;
}
DECLARE_HOOK(HOOK_INIT, restore_state, HOOK_PRIO_DEFAULT);

static void preserve_state(void)
{
	struct test_util_tag tag;
	tag.error_count = __test_error_count;
	system_add_jump_tag(TEST_UTIL_SYSJUMP_TAG, TEST_UTIL_SYSJUMP_VERSION,
			    sizeof(tag), &tag);
}
DECLARE_HOOK(HOOK_SYSJUMP, preserve_state, HOOK_PRIO_DEFAULT);

static int command_run_test(int argc, const char **argv)
{
	run_test(argc, argv);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(runtest, command_run_test, NULL, NULL);

#ifndef CONFIG_ZEPHYR
void z_ztest_run_test_suite(const char *name, struct unit_test *suite)
{
	test_reset();

	while (suite->test) {
		suite->setup();
		RUN_TEST(suite->test);
		suite->teardown();
		suite++;
	}

	/* Sometimes the console task doesn't start until the test is done. */
	sleep(1);

	ccprintf("%s: ", name);

	test_print_result();
}
#endif /* CONFIG_ZEPHYR */
