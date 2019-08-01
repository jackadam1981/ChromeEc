/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Test the STATIC_REQUIRES macro fails on unexpected input. */

#include "common.h"
#include "test_util.h"

#define CONFIG_FOO TEST_VALUE

/* This will cause a compilation error */
STATIC_REQUIRES(CONFIG_FOO) int foo;

void run_test(void)
{
	test_reset();

	/* Nothing to do, observe compilation error */

	test_print_result();
}
