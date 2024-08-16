/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @brief Tests for functions in libc++ (libcxx).
 */

#include "test_util.h"
#include "timer.h"

#include <chrono>

static int test_system_clock()
{
	auto start = std::chrono::system_clock::now();
	udelay(0.5 * SECOND);
	auto end = std::chrono::system_clock::now();
	auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				  end - start)
				  .count();
	TEST_NEAR(elapsed_ms, 500ULL, 5ULL, "%llu");

	return EC_SUCCESS;
}

static int test_aligned_allocation()
{
	int *aligned = new (static_cast<std::align_val_t>(16)) int;
	TEST_ASSERT(aligned != nullptr);
	delete aligned;
	// TODO: check pointer is actually aligned.
	return EC_SUCCESS;
}

void run_test(int, const char **)
{
	test_reset();
	RUN_TEST(test_system_clock);
	RUN_TEST(test_aligned_allocation);
	test_print_result();
}
