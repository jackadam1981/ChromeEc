/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Test if unaligned access works properly
 */

#include "test_util.h"

#include <cstdio>
#include <cstring>

static int test_unaligned_access()
{
	alignas(int32_t)
		constexpr int8_t test_array[7] = { -1, 9, 4, 6, 4, 6, 7 };

	/* This is aligned access */
	const int32_t *test_array_ptr1 =
		reinterpret_cast<const int32_t *>(test_array);
	ccprints("The value is: 0x%08x", test_array_ptr1[0]);

	/* This is unaligned access */
	const int32_t *test_array_ptr2 =
		reinterpret_cast<const int32_t *>(test_array + 1);
	ccprints("The value is: 0x%08x", test_array_ptr2[0]);

	return EC_SUCCESS;
}

extern "C" void run_test(int, const char **)
{
	test_reset();
	RUN_TEST(test_unaligned_access);
	test_print_result();
}
