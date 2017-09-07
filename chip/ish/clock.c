/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks and power management settings */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "hooks.h"
#include "hwtimer.h"
#include "hwtimer_ish.h"
#include "power_mgt.h"
#include "registers.h"
#include "shared_mem.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CLOCK, outstr)
#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ## args)

#ifdef CONFIG_LOW_POWER_IDLE

static int allow_low_power; /* "lowpower off" by default */

static uint32_t idle_sleep_cnt;
static uint32_t idle_deep_sleep_cnt;

static uint64_t actual_sleep;
static uint64_t total_deep_sleep;


#define CONSOLE_IN_USE_ON_BOOT_TIME (15*SECOND)
static int console_in_use_timeout_sec = 60;
static timestamp_t console_expire_time;

/* recovery time from D0ix:
 * eventually, this value should be calculated planned power state.
 */
#define RECOVER_FROM_LOW_POWER (48 * MSEC / 1000)  /* 48 usec, value in counts */
/* delay due to wake timer,timer(2) */
#define WAKE_DELAY_DUE_TO_TIMER (24 * MSEC / 1000) /* 24 usec, value in counts */
#define ENOUGH_TIME_FOR_LOW_POWER (RECOVER_FROM_LOW_POWER + WAKE_DELAY_DUE_TO_TIMER)

#endif /* #ifdef CONFIG_LOW_POWER_IDLE */

void clock_init(void)
{
	/* No initialization for ISH clock since D0ix is not enabled yet */
}


#ifdef CONFIG_LOW_POWER_IDLE
void clock_refresh_console_in_use(void)
{
	disable_sleep(SLEEP_MASK_CONSOLE);

	/* Set console in use expire time. */
	console_expire_time = get_time();
	console_expire_time.val += console_in_use_timeout_sec * SECOND;
}

/**
 * Low power idle task.  Executed when no tasks are ready to be scheduled.
 */
void __idle(void)
{

	timestamp_t t0;

	timestamp_t begin, end;

	uint32_t early_wakeup;
	int32_t max_sleep_time; /*substract 'recovery/resume from sleep'
				   from 'next_evnt_delay'. */
	allow_low_power = 1;    /* for debug,  "lowpower on" by default */

	idle_sleep_cnt = 0;
	idle_deep_sleep_cnt = 0;

	actual_sleep = 0;
	total_deep_sleep = 0;

	console_expire_time.val = get_time().val + CONSOLE_IN_USE_ON_BOOT_TIME;

	pm_init();

	CPRINTS("low power idle task started");
	while (1) {

		/* Disable interrupts */
		interrupt_disable();

		/* Compute event delay */
		t0 = get_time();

		/* __hw_clock_event_get() is next programmed event time */
		early_wakeup = __hw_clock_event_get();


		max_sleep_time = early_wakeup - t0.le.lo
				- ENOUGH_TIME_FOR_LOW_POWER;

		if (DEEP_SLEEP_ALLOWED && max_sleep_time > 0
				&& allow_low_power != 0) {

			idle_deep_sleep_cnt++;

			/* enable early-wake timer */
			__hw_clock_wake_set(early_wakeup);

			begin = get_time();

			/* identify depth of power saving level */
			/* CPU is in Deepsleep from Here */
			pm_execute_idle_flow(max_sleep_time);

			/* disable interrupt immediately */
			__asm__ volatile("cli;\n\t");

			end = get_time();
			actual_sleep = end.val - begin.val;
			total_deep_sleep += actual_sleep;

			/* disable wake timer */
			__hw_clock_wake_clear();

			/* Do prepare 'resume', leave idle task */
			pm_return_from_idle();

		} else {

			idle_sleep_cnt++;
			/* wdt_disable(); TODO: disable watchdog */
                        CPU_ENTER_IDLE();
		}

		interrupt_enable();

	}

}

#ifdef CONFIG_CMD_IDLE_STATS
/**
 * Print low power idle statistics
 */
static int command_idle_stats(int argc, char **argv)
{
	timestamp_t ts = get_time();

	uint64_t ts_val = (uint64_t)ts.val;
	uint64_t actual_s_t = actual_sleep;
	uint64_t total_d_s = total_deep_sleep;

	uint64divmod(&ts_val, MSEC);
	uint64divmod(&actual_s_t, MSEC);
	uint64divmod(&total_d_s, MSEC);


	ccprintf("Num idle calls that sleep:           	%d\n", idle_sleep_cnt);
	ccprintf("Num idle calls that deep-sleep:      	%d\n",
							idle_deep_sleep_cnt);

	ccprintf("Total deep_sleep (time):     	%.3lu (s)\n\n", total_d_s);

	ccprintf("Total ON(time):       	%.3lu (s)\n\n", ts_val);

	ccprintf("*Acural Sleep time in deepsleep(msec) : %lu\n", actual_s_t);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(idlestats, command_idle_stats,
			"",
			"Print last idle stats");

#endif /* defined(CONFIG_CMD_IDLE_STATS) */


/**
 * do_not allow low power idle
*/
static int command_lowpower(int argc, char **argv)
{
	int v;

	if (argc > 1) {
		if (parse_bool(argv[1], &v)) {
			/*
			 * Force deep sleep not to use heavy sleep mode or
			 * allow it to use the heavy sleep mode.
			 */
			if (v) { /* 'on' */
				allow_low_power = 1;
				ccprintf("allow_low_power=1\n");
			} else {   /* 'off' */
				allow_low_power = 0;
				ccprintf("allow_low_power=0\n");
			}
		}
	}

	ccprintf("lowpower : %s\n", allow_low_power ? "on" : "off");

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(lowpower, command_lowpower,
			"[ on | off ]",
			"Enable Low power idle on/off"
			"to enter low power idle mode.\nUse 'on' to "
			"allow deep sleep\n.");

#endif /* CONFIG_LOW_POWER_IDLE */
