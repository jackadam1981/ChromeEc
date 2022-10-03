/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>

#include "common.h"
#include "test_util.h"

test_static int test_abort(void)
{
	ccprintf("test_abort starting\n");
	abort();
	/* Should never reach this. */
	return EC_ERROR_UNKNOWN;
}

void run_test(int argc, const char **argv)
{
	RUN_TEST(test_abort);
	test_print_result();
}
