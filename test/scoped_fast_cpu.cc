/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Basic test of ScopedFastCpu.
 */

/* Fake the clock_enable_module in the header files. */
#define clock_enable_module fake_clock_enable_module

#include "common.h"

extern "C" {
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

test_static int fast_cpu_disable_at_start()
{
	{
		TEST_EQ(fast_cpu_state, 0, "%d");
		{
			/* instantiate, which calls constructor. */
			ScopedFastCpu cpu;
			TEST_EQ(fast_cpu_state, 1, "%d");
			/* destructed here. */
		}
		TEST_EQ(fast_cpu_state, 0, "%d");
	}
	return EC_SUCCESS;
}

test_static int fast_cpu_enable_at_start()
{
	{
		ScopedFastCpu cpu;
		TEST_EQ(fast_cpu_state, 1, "%d");
		{
			/* instantiate, which calls constructor. */
			ScopedFastCpu cpu;
			TEST_EQ(fast_cpu_state, 1, "%d");
			/* destructed here. */
		}
		TEST_EQ(fast_cpu_state, 1, "%d");
	}
	return EC_SUCCESS;
}

extern "C" void run_test(int argc, const char **argv)
{
	test_reset();

	RUN_TEST(fast_cpu_disable_at_start);
	RUN_TEST(fast_cpu_enable_at_start);

	test_print_result();
}
