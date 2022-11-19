/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test_util.h"

extern "C" {
#include "timer.h"
}

#include <chrono>

static int test_system_clock()
{
	auto start = std::chrono::system_clock::now();
	udelay(0.5 * SECOND);
	auto end = std::chrono::system_clock::now();
	auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
				 end - start)
				 .count();
	TEST_NEAR(elapsedMs, 500ULL, 5ULL, "%llu");

	return EC_SUCCESS;
}

extern "C" void run_test(int, const char **)
{
	test_reset();
	RUN_TEST(test_system_clock);
	test_print_result();
}
