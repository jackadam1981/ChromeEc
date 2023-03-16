/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Basic test of std::vector and dynamic memory allocation.
 */

/* Fake the clock_enable_module in the header files. */
#define clock_enable_module fake_clock_enable_module

#include <array>
#include <vector>

extern "C" {
#include "common.h"
#include "console.h"
#include "test_util.h"
}

#include "scoped_fast_cpu.h"

static int fast_cpu_state = 0;

extern "C" void fake_clock_enable_module(enum module_id module, int enable)
{
	if (module == MODULE_FAST_CPU) {
		fast_cpu_state = enable;
	}
}

test_static int fast_cpu_barrier()
{
	TEST_EQ(fast_cpu_state, 0, "%d");
	return EC_SUCCESS;
}

test_static int single_fast_cpu()
{
	ScopedFastCpu fast_cpu;

	TEST_EQ(fast_cpu_state, 1, "%d");

	return EC_SUCCESS;
}

test_static int multiple_fast_cpu()
{
	ScopedFastCpu fast_cpu1;

	TEST_EQ(fast_cpu_state, 1, "%d");

	ScopedFastCpu fast_cpu2;

	TEST_EQ(fast_cpu_state, 1, "%d");

	ScopedFastCpu fast_cpu3;

	TEST_EQ(fast_cpu_state, 1, "%d");

	return EC_SUCCESS;
}

static int fib_fast_cpu(int n)
{
	ScopedFastCpu fast_cpu;

	TEST_EQ(fast_cpu_state, 1, "%d");

	if (n <= 1) {
		return 1;
	}

	TEST_EQ(fast_cpu_state, 1, "%d");

	return fib_fast_cpu(n - 1) + fib_fast_cpu(n - 2);
}

test_static int recursive_fast_cpu()
{
	ScopedFastCpu fast_cpu1;
	TEST_EQ(fib_fast_cpu(5), 8, "%d");
	TEST_EQ(fast_cpu_state, 1, "%d");
	return EC_SUCCESS;
}

extern "C" void run_test(int argc, const char **argv)
{
	test_reset();

	RUN_TEST(fast_cpu_barrier);
	RUN_TEST(single_fast_cpu);
	RUN_TEST(fast_cpu_barrier);
	RUN_TEST(multiple_fast_cpu);
	RUN_TEST(fast_cpu_barrier);
	RUN_TEST(recursive_fast_cpu);
	RUN_TEST(fast_cpu_barrier);

	test_print_result();
}
