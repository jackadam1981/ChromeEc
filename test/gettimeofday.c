/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "mock/timer_mock.h"
#include "test_util.h"

#include <errno.h>
#include <time.h>

int _gettimeofday(struct timeval *tv, void *tz);

static int test_gettimeofday_zero(void)
{
	struct timeval tv;
	timestamp_t now;

	now.val = 0;
	set_time(now);

	TEST_EQ(_gettimeofday(&tv, NULL), 0, "%d");
	TEST_EQ(tv.tv_sec, 0L, "%ld");
	TEST_EQ(tv.tv_usec, 0L, "%ld");

	return EC_SUCCESS;
}

static int test_gettimeofday_zero_seconds(void)
{
	struct timeval tv;
	timestamp_t now;

	now.val = 100;
	set_time(now);

	TEST_EQ(_gettimeofday(&tv, NULL), 0, "%d");
	TEST_EQ(tv.tv_sec, 0L, "%ld");
	TEST_EQ(tv.tv_usec, 100L, "%ld");

	return EC_SUCCESS;
}

static int test_gettimeofday_nonzero_seconds(void)
{
	struct timeval tv;
	timestamp_t now;

	now.val = 1000001;
	set_time(now);

	TEST_EQ(_gettimeofday(&tv, NULL), 0, "%d");
	TEST_EQ(tv.tv_sec, 1L, "%ld");
	TEST_EQ(tv.tv_usec, 1L, "%ld");

	return EC_SUCCESS;
}

static int test_gettimeofday_max(void)
{
	struct timeval tv;
	timestamp_t now;

	now.val = UINT64_MAX;
	set_time(now);

	TEST_EQ(_gettimeofday(&tv, NULL), 0, "%d");
	TEST_EQ(tv.tv_sec, 18446744073709L, "%ld");
	TEST_EQ(tv.tv_usec, 551615L, "%ld");

	return EC_SUCCESS;
}

static int test_gettimeofday_null_arg(void)
{
	TEST_EQ(_gettimeofday(NULL, NULL), -1, "%d");
	TEST_EQ(errno, EFAULT, "%d");

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();
	RUN_TEST(test_gettimeofday_zero);
	RUN_TEST(test_gettimeofday_zero_seconds);
	RUN_TEST(test_gettimeofday_nonzero_seconds);
	RUN_TEST(test_gettimeofday_max);
	RUN_TEST(test_gettimeofday_null_arg);
	test_print_result();
}
