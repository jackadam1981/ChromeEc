/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test USB common module.
 */
#include "atomic.h"
#include "test_util.h"
#include "usb_pd_timer.h"

int test_pd_timers_uint64_t(void)
{
	int bit;

	/*
	 * Initialization calling pd_timer_init will
	 * initialize the port's active timer to be
	 * clear and disabled timer to be set for all
	 * mask bits
	 */
	pd_timer_init(0);
	for (bit = 0; bit < PD_TIMER_COUNT; ++bit)
		TEST_EQ((PD_CHK_ACTIVE(0, 1ULL << bit) == 0),
			1, "%d");
	for (bit = 0; bit < PD_TIMER_COUNT; ++bit)
		TEST_EQ((PD_CHK_DISABLED(0, 1ULL << bit) != 0),
			1, "%d");

	/*
	 * Set one active bit at a time and verify it
	 * is the only bit set. Reset the bit on each
	 * iteration of the bit loop.
	 */
	for (bit = 0; bit < PD_TIMER_COUNT; ++bit) {
		TEST_EQ((PD_CHK_ACTIVE(0, 1ULL << bit) == 0),
			1, "%d");
		PD_SET_ACTIVE(0, 1ULL << bit);
		for (int i = 0; i < PD_TIMER_COUNT; ++i) {
			if (i != bit)
				TEST_EQ((PD_CHK_ACTIVE(0, 1ULL << i) == 0),
					1, "%d");
			else
				TEST_EQ((PD_CHK_ACTIVE(0, 1ULL << i) != 0),
					1, "%d");
		}
		PD_CLR_ACTIVE(0, 1ULL << bit);
	}

	/*
	 * Clear one disabled bit at a time and verify it
	 * is the only bit clear. Reset the bit on each
	 * iteration of the bit loop.
	 */
	for (bit = 0; bit < PD_TIMER_COUNT; ++bit) {
		TEST_EQ((PD_CHK_DISABLED(0, 1ULL << bit) != 0),
			1, "%d");
		PD_CLR_DISABLED(0, 1ULL << bit);
		for (int i = 0; i < PD_TIMER_COUNT; ++i) {
			if (i != bit)
				TEST_EQ((PD_CHK_DISABLED(0, 1ULL << i) != 0),
					1, "%d");
			else
				TEST_EQ((PD_CHK_DISABLED(0, 1ULL << i) == 0),
					1, "%d");
		}
		PD_SET_DISABLED(0, 1ULL << bit);
	}

	return EC_SUCCESS;
}

int test_pd_timers(void)
{
	int bit;
	int ms_to_expire;

	/*
	 * Initialization calling pd_timer_init will
	 * initialize the port's active timer to be
	 * clear and disabled timer to be set for all
	 * mask bits
	 */
	pd_timer_init(0);

	/*
	 * Verify all timers are disabled
	 */
	for (bit = 0; bit < PD_TIMER_COUNT; ++bit)
		TEST_EQ(pd_timer_is_disabled(0, bit),
			1, "%d");

	/*
	 * Enable some timers
	 */
	for (bit = 0; bit < 5; ++bit)
		pd_timer_enable(0, bit, (bit + 1) * 50);

	/*
	 * Verify all timers for enabled/disabled
	 */
	for (bit = 0; bit < PD_TIMER_COUNT; ++bit) {
		if (bit < 5)
			TEST_EQ(pd_timer_is_disabled(0, bit),
				0, "%d");
		else
			TEST_EQ(pd_timer_is_disabled(0, bit),
				1, "%d");
	}

	/*
	 * Disable the first timer
	 */
	pd_timer_disable(0, 0);

	/*
	 * Verify all timers for enabled/disabled
	 */
	for (bit = 0; bit < PD_TIMER_COUNT; ++bit) {
		if (bit > 0 && bit < 5)
			TEST_EQ(pd_timer_is_disabled(0, bit),
				0, "%d");
		else
			TEST_EQ(pd_timer_is_disabled(0, bit),
				1, "%d");
	}

	/*
	 * Verify finding the next timer to expire
	 *
	 * Timer at BIT(1) is the next to expire and originally
	 * had an expire time of 100ms. So allow for the test's
	 * simulated time lapse and verify in the 90-100 range.
	 */
	ms_to_expire = pd_timer_next_expiration(0);
	TEST_GE(ms_to_expire, 90, "%d");
	TEST_LE(ms_to_expire, 100, "%d");

	/*
	 * Enable the timers in the PR range
	 */
	for (bit = PR_TIMER_START; bit <= PR_TIMER_END; ++bit)
		pd_timer_enable(0, bit, 5);

	/*
	 * Verify all timers for enabled/disabled
	 */
	for (bit = 0; bit < PD_TIMER_COUNT; ++bit) {
		if ((bit > 0 && bit < 5) ||
		    (bit >= PR_TIMER_START && bit <= PR_TIMER_END))
			TEST_EQ(pd_timer_is_disabled(0, bit),
				0, "%d");
		else
			TEST_EQ(pd_timer_is_disabled(0, bit),
				1, "%d");
	}

	/*
	 * Disable the PR timer range
	 */
	pd_timer_disable_range(0, PR_TIMER_RANGE);

	/*
	 * Verify all timers for enabled/disabled
	 */
	for (bit = 0; bit < PD_TIMER_COUNT; ++bit) {
		if (bit > 0 && bit < 5)
			TEST_EQ(pd_timer_is_disabled(0, bit),
				0, "%d");
		else
			TEST_EQ(pd_timer_is_disabled(0, bit),
				1, "%d");
	}

	/*
	 * Disable the PE timer range
	 */
	pd_timer_disable_range(0, PE_TIMER_RANGE);

	/*
	 * Verify all timers are disabled
	 */
	for (bit = 0; bit < PD_TIMER_COUNT; ++bit)
		TEST_EQ(pd_timer_is_disabled(0, bit),
			1, "%d");

	return EC_SUCCESS;
}

void run_test(int argc, char **argv)
{
	RUN_TEST(test_pd_timers_uint64_t);
	RUN_TEST(test_pd_timers);

	test_print_result();
}
