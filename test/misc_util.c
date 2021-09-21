/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test mis_util from the utils directory
 */

#include "common.h"
#include "util/misc_util.h"
#include "test_util.h"

static int test_parse_hex_string(void)
{
	uint8_t buf[UINT8_MAX];
	int len = ARRAY_SIZE(buf);
	char *evenString = "0123456789";
	char *oddString = "123456789";
	char *emptyString = "";
	char *nonsenseString = "fish";
	uint8_t expected_results[] = { 0x01, 0x23, 0x45, 0x67, 0x89 };

	/* Verify an empty string returns error */
	TEST_ASSERT(parse_hex_string(buf, &len, emptyString) == -1);

	/* Verify an insufficient buffer returns error */
	len = 2;
	TEST_ASSERT(parse_hex_string(buf, &len, evenString) == -1);

	/* Verify a non-hex string returns error */
	len = ARRAY_SIZE(buf);
	TEST_ASSERT(parse_hex_string(buf, &len, nonsenseString) == -1);

	/* Verify an even-length string parses successfully */
	len = ARRAY_SIZE(buf);
	TEST_ASSERT(parse_hex_string(buf, &len, evenString) == 0);
	TEST_ASSERT(len == ARRAY_SIZE(expected_results));
	TEST_ASSERT_ARRAY_EQ(buf, expected_results, len);

	/* Verify an odd-length string is parsed with a 0 padded in front */
	len = ARRAY_SIZE(buf);
	TEST_ASSERT(parse_hex_string(buf, &len, oddString) == 0);
	TEST_ASSERT(len == ARRAY_SIZE(expected_results));
	TEST_ASSERT_ARRAY_EQ(buf, expected_results, len);

	return EC_SUCCESS;
}

void run_test(int argc, char **argv)
{
	test_reset();

	RUN_TEST(test_parse_hex_string);

	test_print_result();
}
