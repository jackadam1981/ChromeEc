/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <exception>
#include "common.h"
#include "test_util.h"

test_static void throw_exception()
{
	throw std::exception();
}

test_static int test_exception()
{
	try {
		ccprintf("Calling throw_exception()\n");
		throw_exception();
	} catch (std::exception &e) {
		/*
		 * Since we have exceptions disabled, we should not reach this.
		 * Instead, the exception should cause a reboot.
		 */
		ccprintf("Caught exception\n");
		return EC_ERROR_UNKNOWN;
	}
	return EC_ERROR_UNKNOWN;
}

extern "C" void run_test(int argc, const char **argv)
{
	RUN_TEST(test_exception);
	test_print_result();
}
