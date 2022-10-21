/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Basic test of std::vector and dynamic memory allocation.
 */

#include <vector>

extern "C" {
#include "common.h"
#include "test_util.h"
}

test_static int push_back_elements()
{
	std::vector<int> vec;

	vec.push_back(0);
	vec.push_back(1);
	vec.push_back(2);
	vec.push_back(3);

	TEST_EQ(static_cast<int>(vec.size()), 4, "%d");
	TEST_EQ(vec[0], 0, "%d");
	TEST_EQ(vec[1], 1, "%d");
	TEST_EQ(vec[2], 2, "%d");
	TEST_EQ(vec[3], 3, "%d");

	return EC_SUCCESS;
}

test_static int fill_vector()
{
	// This test allocates 32kB of memory in total
	constexpr int num_elements = 8 * 1024;
	std::vector<int> vec;

	for (int i = 0; i < num_elements; ++i)
		vec.push_back(i);

	TEST_EQ(static_cast<int>(vec.size()), num_elements, "%d");
	// Ideally we should test all the values, however each TEST_EQ
	// prints on console and the test crashes if I try to print 8k lines.
	// for (int i=0; i < num_elements; ++i)
	// 	TEST_EQ(vec[i], i, "%d");

	return EC_SUCCESS;
}

extern "C" void run_test(int argc, const char **argv)
{
	test_reset();

	RUN_TEST(push_back_elements);
	RUN_TEST(fill_vector);

	test_print_result();
}
