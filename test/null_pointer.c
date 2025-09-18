/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "panic.h"
#include "system.h"
#include "test_util.h"

#include <stdlib.h>

test_static int test_null_pointer_dereference(void)
{
	volatile uint32_t *null_ptr = NULL;
	ccprintf("The value of null_ptr after dereferencing is: %d\r\n",
		 *null_ptr);

	/* No null pointer checking in EC. */
	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();
	RUN_TEST(test_null_pointer_dereference);
	test_print_result();
}
