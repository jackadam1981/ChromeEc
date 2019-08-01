/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Test the IS_ENABLED and STATIC_REQUIRES macros */
#include "common.h"
#include "test_util.h"

#undef	CONFIG_UNDEFINED
#define	CONFIG_BLANK

static int test_is_enabled_undef(void)
{
	TEST_ASSERT(IS_ENABLED(CONFIG_UNDEFINED) == 0);

	return EC_SUCCESS;
}

static int test_is_enabled_blank(void)
{
	TEST_ASSERT(IS_ENABLED(CONFIG_BLANK) == 1);

	return EC_SUCCESS;
}

STATIC_REQUIRES(CONFIG_UNDEFINED) int this_var_is_extern;
STATIC_REQUIRES(CONFIG_BLANK) int this_var_is_static;

static int test_static_requires_blank(void)
{
	TEST_ASSERT(this_var_is_static == 0);

	return EC_SUCCESS;
}

static int test_static_requires_unused_no_fail(void)
{
	if (IS_ENABLED(CONFIG_UNDEFINED))
		this_var_is_extern = 1;

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_is_enabled_undef);
	RUN_TEST(test_is_enabled_blank);
	RUN_TEST(test_static_requires_blank);
	RUN_TEST(test_static_requires_unused_no_fail);

	test_print_result();
}
