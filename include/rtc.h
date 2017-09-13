/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* RTC cross-platform functions */

#ifndef __CROS_EC_RTC_H
#define __CROS_EC_RTC_H

#include "common.h"

#define SECS_PER_DAY        (60 * 60 * 24)
#define SECS_PER_YEAR       (365 * SECS_PER_DAY)
/* The seconds elapsed from 01-01-1970 to 01-01-2000 */
#define SECS_TILL_YEAR_2K   (946684800)
#define IS_LEAP_YEAR(x)     \
	(((x) % 4 == 0) && (((x) % 100 != 0) || ((x) % 400 == 0)))

struct calandar_date {
	int year; /* years since 2000 A.C. */
	int month;
	int day;
};

/**
 * Convert calandar date to seconds elapsed since epoch time.
 *
 * @param time  The calandar date (years, months, and days).
 * @return the seconds elapsed since epoch time (01-01-1970 00:00:00).
 */
uint32_t date_to_sec(struct calandar_date time);

/**
 * Convert seconds elapsed since epoch time to calandar date
 *
 * @param sec  The seconds elapsed since epoch time (01-01-1970 00:00:00).
 * @return the calandar date (years, months, and days).
 */
struct calandar_date sec_to_date(uint32_t sec);

#endif /* __CROS_EC_RTC_H */
