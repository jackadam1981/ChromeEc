/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "test_util.h"
#include "rollback.h"

test_static int get_min_version(void)
{
	int ver = rollback_get_minimum_version();
	ccprintf("GET MINNN VERSION: %d\n", ver);
	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(get_min_version);

	test_print_result();
}
