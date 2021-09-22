/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test USB common module.
 */
#include "atomic.h"
#include "test_util.h"
#include "usb_pd_timer.h"

int test_pd_timer(void)
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

void run_test(int argc, char **argv)
{
	RUN_TEST(test_pd_timer);

	test_print_result();
}
