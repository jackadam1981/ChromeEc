/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <weaver_ng.h>
#include "test_util.h"

static int dummy_test(void)
{
	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(dummy_test);

	test_print_result();
}
