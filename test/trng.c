/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Does a basic test of a chip's trng implementation.
 */

#include "common.h"
#include "console.h"
#include "test_util.h"
#include "util.h"
#include "trng.h"
#include "task.h"

static int test_rand(void)
{
	int i;
	for (i = 0; i < 256; i++)
		rand();
	/* We didn't crash --> Success */
	return EC_SUCCESS;
}

static int test_rand_bytes_aligned(void)
{
	static uint8_t buf[50 * sizeof(void *)];

	rand_bytes(buf, sizeof(buf));
	/* We didn't crash --> Success */
	return EC_SUCCESS;
}

static int test_rand_bytes_unaligned_large(void)
{
	static uint8_t buf[50 * sizeof(void *) - 1];

	rand_bytes(buf, sizeof(buf));
	/* We didn't crash --> Success */
	return EC_SUCCESS;
}


static void die(void) {
	volatile uint32_t * p = (volatile uint32_t *)0x888778787;
	*p = 8;
}

static int test_rand_bytes_unaligned_small(void)
{
	static uint8_t buf[sizeof(void *) - 1];

	rand_bytes(buf, sizeof(buf));
	/* We didn't crash --> Success */

	test_expect_reboot(true, TEST_STATE_PASSED);
	die();
	return EC_SUCCESS;
}

void run_test(void)
{
	// msleep(30); /* Wait for TASK_ID_TEST to initialize */
	task_wake(TASK_ID_TEST);
}

void test_run_step(uint32_t state)
{
	if (state & TEST_STATE_MASK(TEST_STATE_STEP_1)) {
		init_trng();
#ifdef HIDE_EC_STDLIB
		ccprints("WARNING: The chip's rand may not be used");
#endif

		RUN_TEST(test_rand);
		RUN_TEST(test_rand_bytes_aligned);
		RUN_TEST(test_rand_bytes_unaligned_large);
		RUN_TEST(test_rand_bytes_unaligned_small);

		test_print_result();
	}
}