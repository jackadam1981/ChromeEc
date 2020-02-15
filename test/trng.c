/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Does a basic test of a chip's trng implementation.
 */

#include <stdint.h>

#include "common.h"
#include "console.h"
#include "test_util.h"
#include "util.h"
#include "trng.h"
#include "task.h"

static uint32_t hist_bin[512];
static uint32_t hist_bin_size = ((((uint64_t)1) << (sizeof(uint32_t)*8)) / (uint64_t)ARRAY_SIZE(hist_bin));
static const uint32_t hist_samples = 1024 * 1024;
static const uint32_t hist_expected_value = hist_samples / ARRAY_SIZE(hist_bin);

test_static void hist_reset(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(hist_bin); i++) {
		hist_bin[i] = 0;
	}
}

test_static void hist_add(uint32_t v)
{
	hist_bin[v / hist_bin_size]++;
}

test_static int abs(int a) {
	if (a >= 0)
		return a;
	else
		return -a;
}

test_static void hist_print(void)
{
	uint32_t diff_min = UINT32_MAX;
	uint32_t diff_max = 0;
	uint32_t diff;
	uint32_t diff_abs;

	int i;
	test_cprintf("Bin counts:\n");
	for (i = 0; i < ARRAY_SIZE(hist_bin); i++) {
		if ((i != 0) && (i % 32 == 0)) {
			test_cprintf("\n");
			cflush();
		}
		test_cprintf("%-5u ", hist_bin[i]);
	}
	test_cprintf("\n");

	test_cprintf("Bin counts -- difference from expected:\n");
	for (i = 0; i < ARRAY_SIZE(hist_bin); i++) {
		diff = hist_bin[i] - hist_expected_value;
		diff_abs = abs(diff);

		if (diff_min > diff_abs) {
			diff_min = diff_abs;
		}
		if (diff_max < diff_abs) {
			diff_max = diff_abs;
		}

		if ((i != 0) && (i % 32 == 0)) {
			test_cprintf("\n");
			cflush();
		}
		test_cprintf("%-5d ", diff);
	}
	test_cprintf("\n\n");

	test_cprintf("diff_min = %u\n", diff_min);
	test_cprintf("diff_max = %u\n", diff_max);
	test_cprintf("diff_max / bin_size = %u / %u\n", diff_max, hist_expected_value);
}

test_static int test_rand_uniform(void)
{
	uint32_t i;

	hist_reset();
	for (i = 0; i < hist_samples; i++) {
		hist_add(rand());
	}
	hist_print();

	return EC_SUCCESS;
}

test_static int test_rand(void)
{
	int i;

	for (i = 0; i < 256; i++)
		rand();
	/* We didn't crash --> Success */
	return EC_SUCCESS;
}

test_static int test_rand_bytes_aligned(void)
{
	static uint8_t buf[50 * sizeof(void *)];

	rand_bytes(buf, sizeof(buf));
	/* We didn't crash --> Success */
	return EC_SUCCESS;
}

test_static int test_rand_bytes_unaligned_subwordsize(void)
{
	static uint8_t buf[sizeof(void *) - 1];

	rand_bytes(buf, sizeof(buf));
	/* We didn't crash --> Success */

	return EC_SUCCESS;
}

test_static int test_rand_bytes_unaligned_large(void)
{
	static uint8_t buf[50 * sizeof(void *) - 1];

	rand_bytes(buf+1, sizeof(buf)-1);
	/* We didn't crash --> Success */
	return EC_SUCCESS;
}

void run_test(void)
{
#ifdef HIDE_EC_STDLIB
		ccprints("WARNING: The chip's rand may not be used");
#endif
	// msleep(30); /* Wait for TASK_ID_TEST to initialize */
	task_wake(TASK_ID_TEST);
}

void test_run_step(uint32_t state)
{
	if (state & TEST_STATE_MASK(TEST_STATE_STEP_1)) {
		test_expect_reboot(false, TEST_STATE_FAILED);
		init_trng();
		test_expect_reboot(false, TEST_STATE_FAILED);
		RUN_TEST(test_rand);
		test_expect_reboot(false, TEST_STATE_FAILED);
		RUN_TEST(test_rand_bytes_aligned);
		test_expect_reboot(false, TEST_STATE_FAILED);
		RUN_TEST(test_rand_bytes_unaligned_subwordsize);
		test_expect_reboot(false, TEST_STATE_FAILED);
		RUN_TEST(test_rand_bytes_unaligned_large);
		test_expect_reboot(false, TEST_STATE_FAILED);
		RUN_TEST(test_rand_uniform);


		test_cprintf("Test Summary: ");
		test_expect_reboot(false, TEST_STATE_FAILED);
		exit_trng();
		test_print_result();
		test_reset();
	}
}