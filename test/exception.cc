/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <exception>
#include "common.h"
#include "test_util.h"

test_static int test_exception()
{
	throw std::exception();
	return EC_ERROR_UNKNOWN;
}

extern "C" void run_test(int argc, const char **argv)
{
	RUN_TEST(test_exception);
	test_print_result();
}
