/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test the IS_ENABLED macro.
 */
#include "common.h"
#include "compile_time_macros.h"
#include "test_util.h"

#undef	CONFIG_UNDEFINED
#define	CONFIG_BLANK

static int test_undef(void)
{
	TEST_ASSERT(IS_ENABLED(CONFIG_UNDEFINED) == 0);

	return EC_SUCCESS;
}

static int test_blank(void)
{
	TEST_ASSERT(IS_ENABLED(CONFIG_BLANK) == 1);

	return EC_SUCCESS;
}

static struct {
	int member_enabled[IS_ENABLED(CONFIG_BLANK)];
	int member_disabled[IS_ENABLED(CONFIG_UNDEFINED)];
} usage_outside_function;

static int test_usage_outside_function(void)
{
	TEST_ASSERT(ARRAY_SIZE(usage_outside_function.member_enabled) == 1);
	TEST_ASSERT(ARRAY_SIZE(usage_outside_function.member_disabled) == 0);

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_undef);
	RUN_TEST(test_blank);
	RUN_TEST(test_usage_outside_function);

	test_print_result();
}
