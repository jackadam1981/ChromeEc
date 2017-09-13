/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* RTC cross-platform code for Chrome EC */

#include "rtc.h"

static uint32_t days_since_year_start[12] = {
0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

/* Conversion between calandar date and seconds eclapsed since 1970-01-01 */
uint32_t date_to_sec(struct calandar_date time)
{
	int i;
	uint32_t sec;

	sec = time.year * SECS_PER_YEAR;
	for (i = 0; i < time.year; i++) {
		if (IS_LEAP_YEAR(i))
			sec += SECS_PER_DAY;
	}
	time.day += days_since_year_start[time.month - 1];
	time.day += (IS_LEAP_YEAR(time.year) && time.month > 2);
	sec += (time.day - 1) * SECS_PER_DAY;
	/* add the accumulated time in seconds from 1970 to 2000 */
	return sec + SECS_TILL_YEAR_2K;
}

struct calandar_date sec_to_date(uint32_t sec)
{
	struct calandar_date time;
	uint8_t is_leap_year;
	int i;

	/* rtc time must be after year 2000 */
	sec = (sec > SECS_TILL_YEAR_2K) ? (sec - SECS_TILL_YEAR_2K) : 0;

	time.day = sec / SECS_PER_DAY;
	time.year = time.day / 365;
	is_leap_year = IS_LEAP_YEAR(time.year);
	time.day %= 365;
	for (i = 0; i < time.year; i++) {
		if (IS_LEAP_YEAR(i))
			time.day -= 1;
	}
	time.day++;
	if (time.day <= 0) {
		time.year -= 1;
		is_leap_year = IS_LEAP_YEAR(time.year);
		time.day += is_leap_year ? 366 : 365;
	}
	for (i = 1; i < 12; i++) {
		if (days_since_year_start[i] +
		    (is_leap_year && (i >= 2)) >= time.day)
			break;
	}
	time.month = i;

	time.day -= days_since_year_start[time.month - 1] +
		    (is_leap_year && (time.month > 2));
	return time;
}
