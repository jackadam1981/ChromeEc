/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test the IS_ENABLED macro fails on unexpected input.
 */
#include "common.h"
#include "test_util.h"

#define	CONFIG_FIVE		5

test_static int test_five(void)
{
	/* This will cause a compilation error */
	TEST_ASSERT(IS_ENABLED(CONFIG_FIVE) == 0);

	return EC_ERROR_UNKNOWN;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_five);

	test_print_result();
}
