// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "atomic.h"
#include "system.h"
#include "timer.h"

#include <zephyr/ztest.h>

static atomic_t interrupt_counter;
static const int rtc_delay_seconds = 1;
static atomic_t rtc_fired;

ZTEST_SUITE(rtc_npcx9, NULL, NULL, NULL, NULL, NULL);

ZTEST(rtc_npcx9, test_rtc_alarm_fired)
{
	atomic_clear(&interrupt_counter);
	system_set_rtc_alarm(rtc_delay_seconds, 0);

	crec_sleep(2 * rtc_delay_seconds);

	rtc_fired = atomic_get(&interrupt_counter);

	zassert_equal(1, rtc_fired);
	zassert_equal(0, system_get_rtc_alarm());
}
