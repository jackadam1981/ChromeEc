/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test_util.h"
#define SECOND 1000000

struct rtc_time_reg {
	uint32_t rtc_ssr; /* subseconds */
	uint32_t rtc_tr; /* hours, minutes, seconds */
	uint32_t rtc_dr; /* years, months, dates, week days */
};

int32_t rtcss_to_us(uint32_t rtcss)
{
	/* Assume 1 ss == 1 us for testing */
	return rtcss;
}

uint32_t get_rtc_diff(const struct rtc_time_reg *rtc0,
		      const struct rtc_time_reg *rtc1)
{
	uint32_t rtc0_val, rtc1_val, diff;

	rtc0_val = (rtc0->rtc_tr & 0xF) * SECOND + rtcss_to_us(rtc0->rtc_ssr);
	rtc1_val = (rtc1->rtc_tr & 0xF) * SECOND + rtcss_to_us(rtc1->rtc_ssr);
	diff = rtc1_val;
	if (rtc1_val < rtc0_val) {
		/* rtc_ssr has wrapped, since we assume rtc0 < rtc1, add
		 * 10 seconds to get the correct value
		 */
		diff += 10 * SECOND;
	}
	diff -= rtc0_val;
	return diff;
}

static int test_get_rtc_diff(void)
{
	struct rtc_time_reg rtc0;
	struct rtc_time_reg rtc1;

	/* Test basic functionality */
	rtc0.rtc_tr = 0;
	rtc1.rtc_tr = 1;
	rtc0.rtc_ssr = SECOND >> 1; /* 1/2 second */
	rtc1.rtc_ssr = SECOND >> 2; /* 1/4 second */

	/* Total difference should be 1 1/4 second */
	TEST_ASSERT(get_rtc_diff(&rtc0, &rtc1) == 3 * (SECOND >> 2));

	/* Test wrapping behavior */
	rtc0.rtc_tr = 7;
	rtc1.rtc_tr = 3;
	rtc0.rtc_ssr = SECOND >> 2;
	rtc1.rtc_ssr = SECOND >> 2;

	/* We wrapped from 7 around 10 to 3; so it's 6 seconds later */
	TEST_ASSERT(get_rtc_diff(&rtc0, &rtc1) == 6 * SECOND);

	return EC_SUCCESS;
}

void run_test(void)
{
	RUN_TEST(test_get_rtc_diff);
	test_print_result();
}
