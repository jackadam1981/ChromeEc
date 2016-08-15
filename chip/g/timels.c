/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "console.h"
#include "hooks.h"
#include "hwtimer.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"

/*
 * It takes a couple hundred uS to resume for sleep. Disable sleep if an
 * event is happening too soon.
 */
#define TIMER_MIN_US 500
#define TIMER_MAX 0xffffffff
/* The frequency of timerls is 256k so there are about 4ticks/usec */
#define TICKS_PER_USEC 4

static int timer_updated;
static uint32_t max_hwtime_us;

static inline uint64_t ticks_to_usec(uint32_t ticks)
{
	return ticks * TICKS_PER_USEC;
}

static inline uint32_t usec_to_ticks(uint32_t next_evt_us)
{
	return next_evt_us / TICKS_PER_USEC;
}

static inline uint32_t timels_get_timer_value(void)
{
	return GREG32(TIMELS, TIMER0_VALUE);
}

static void timels_set_alarm(uint32_t next_evt_us)
{
	GREG32(TIMELS, TIMER0_LOAD) = usec_to_ticks(next_evt_us);
}

int timels_setup_sleep(int is_deep_sleep)
{
	uint32_t current_time = get_time().le.lo;
	uint32_t event_time = __hw_clock_event_get();
	uint32_t next_evt_us = event_time - current_time;

	if (!max_hwtime_us) {
		max_hwtime_us = __hw_clock_source_get_max();
		/*
		 * Needs to be true for the condition
		 * next_evt_us > max_hwtime_us to correctly detect wrapping.
		 */
		ASSERT(max_hwtime_us < 0xffffffff / 2);
	}

	/*
	 * If the next_event_us greater than the maximum value of the hwtimer
	 * the hwtimer wrapped. Update next_evt_us to the correct value.
	 */
	if (next_evt_us > max_hwtime_us)
		next_evt_us = max_hwtime_us - current_time + event_time;

	/*
	 * In deep sleep these events dont matter and the clock is reset on
	 * resume so disable the timer.
	 */
	if (is_deep_sleep) {
		GWRITE_FIELD(TIMELS, TIMER0_CONTROL, ENABLE, 0);
		return EC_SUCCESS;
	}

	/*
	 * The event should be far enough in the future that the chip can go to
	 * sleep and resume.
	 */
	if (next_evt_us < TIMER_MIN_US) {
		delay_sleep_by(TIMER_MIN_US);
		GWRITE_FIELD(TIMELS, TIMER0_CONTROL, ENABLE, 0);
		return EC_ERROR_UNKNOWN;
	}
	/* Wake up when for the next event */
	timels_set_alarm(next_evt_us);

	/* Enable the timer */
	GWRITE_FIELD(TIMELS, TIMER0_CONTROL, ENABLE, 1);

	timer_updated = 0;
	return EC_SUCCESS;
}

static uint64_t timels_get_diff_us(void)
{
	uint32_t start = GREG32(TIMELS, TIMER0_LOAD);
	uint64_t wrap_time = 0;

	if (GREAD_FIELD(TIMELS, TIMER0_STATUS, WRAPPED))
		wrap_time = ticks_to_usec(start);

	return ticks_to_usec(start - timels_get_timer_value()) + wrap_time;
}

void timels_update_hw_timer(void)
{
	timestamp_t new_time;

	if (timer_updated)
		return;

	timer_updated = 1;
	/* Disable interrupts while changing the hw timer */
	interrupt_disable();

	/* Calculate the sleep time in usec and add it to the hw timer */
	new_time = get_time();
	new_time.val += timels_get_diff_us();
	force_time(new_time);

	interrupt_enable();
}

void timels_init(void)
{
	/* Reset the low speed clock */
	GREG32(TIMELS, TIMER0_LOAD) = TIMER_MAX;

	/* Enable timer0 */
	GREG32(TIMELS, TIMER0_RELOADVAL) = TIMER_MAX;
	GWRITE_FIELD(TIMELS, TIMER0_CONTROL, WRAP, 1);
	GWRITE_FIELD(TIMELS, TIMER0_CONTROL, ENABLE, 0);
}
DECLARE_HOOK(HOOK_INIT, timels_init, HOOK_PRIO_DEFAULT);
