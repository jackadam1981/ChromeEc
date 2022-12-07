/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_UNITSTD_H__
#define __CROS_EC_UNITSTD_H__

/* Time units in microseconds */
#define SECOND 1000000

/**
 * Sleep.
 *
 * The current task will be de-scheduled for at least the specified delay (and
 * perhaps longer, if a higher-priority task is running when the delay
 * expires).
 *
 * This may only be called from a task function, with interrupts enabled.
 *
 * @param us		Number of microseconds to sleep.
 */
int usleep(unsigned us);

/**
 * Sleep for seconds
 *
 * Otherwise the same as usleep().
 *
 * @param sec		Number of seconds to sleep.
 */
static inline unsigned int sleep(unsigned sec)
{
	usleep(sec * SECOND);
	return 0;
}

#endif /* __CROS_EC_UNISTD_H__ */
