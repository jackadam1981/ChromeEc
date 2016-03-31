/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks and power management settings */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "hwtimer.h"
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
/* Recovery time for HvySlp2 is 0 usec */
#define HEAVY_SLEEP_RECOVER_TIME_USEC   75

#define SET_HTIMER_DELAY_USEC           200

static int idle_sleep_cnt;
static int idle_dsleep_cnt;
static uint64_t total_idle_dsleep_time_us;

/*
 * Fixed amount of time to keep the console in use flag true after boot in
 * order to give a permanent window in which the heavy sleep mode is not used.
 */
#define CONSOLE_IN_USE_ON_BOOT_TIME (15*SECOND)
static int console_in_use_timeout_sec = 60;
static timestamp_t console_expire_time;
#endif /*CONFIG_LOW_POWER_IDLE */

static int freq = 120000000;

void clock_wait_cycles(uint32_t cycles)
{
}

int clock_get_freq(void)
{
	return freq;
}

void clock_init(void)
{
}

#ifdef CONFIG_LOW_POWER_IDLE
/**
 * initialization of Hibernation timer
 */
static void htimer_init(void)
{
}

/**
 * Use hibernate module to set up an htimer interrupt at a given
 * time from now
 *
 * @param seconds      Number of seconds before htimer interrupt
 * @param microseconds Number of microseconds before htimer interrupt
 */
static void system_set_htimer_alarm(uint32_t seconds, uint32_t microseconds)
{
}

/**
 * return time slept in micro-seconds
 */
static timestamp_t system_get_htimer(void)
{
	timestamp_t time = 0;

	return time;  /* in uSec */
}

/**
 * Disable and clear hibernation timer interrupt
 */
static void system_reset_htimer_alarm(void)
{
}

static void prepare_for_deep_sleep(void)
{
}

static void resume_from_deep_sleep(void)
{
}


void clock_refresh_console_in_use(void)
{
}

/**
 * Low power idle task.  Executed when no tasks are ready to be scheduled.
 */
void __idle(void)
{
	timestamp_t t0;
	timestamp_t t1;
	timestamp_t ht_t1;
	uint32_t next_delay;
	uint32_t max_sleep_time;
	int time_for_dsleep;
	int uart_ready_for_deepsleep;

	htimer_init(); /* hibernation timer initialize */

	disable_sleep(SLEEP_MASK_CONSOLE);
	console_expire_time.val = get_time().val + CONSOLE_IN_USE_ON_BOOT_TIME;


	/*
	 * Print when the idle task starts.  This is the lowest priority task,
	 * so this only starts once all other tasks have gotten a chance to do
	 * their task inits and have gone to sleep.
	 */
	CPRINTS("low power idle task started");

	while (1) {
		/* Disable interrupts */
		interrupt_disable();

		t0 = get_time();  /* uSec */

		/* __hw_clock_event_get() is next programmed timer event */
		next_delay = __hw_clock_event_get() - t0.le.lo;

		time_for_dsleep = next_delay > (HEAVY_SLEEP_RECOVER_TIME_USEC +
						SET_HTIMER_DELAY_USEC);

		max_sleep_time = next_delay - HEAVY_SLEEP_RECOVER_TIME_USEC;

		/* check if there enough time for deep sleep */
		if (DEEP_SLEEP_ALLOWED && time_for_dsleep) {


			/*
			 * Check if the console use has expired and console
			 * sleep is masked by GPIO(UART-RX) interrupt.
			 */
			if ((sleep_mask & SLEEP_MASK_CONSOLE) &&
					t0.val > console_expire_time.val) {
				/* allow console to sleep. */
				enable_sleep(SLEEP_MASK_CONSOLE);

				/*
				 * Wait one clock before checking if heavy sleep
				 * is allowed to give time for sleep mask
				 * to be updated.
				 */
				clock_wait_cycles(1);

				if (LOW_SPEED_DEEP_SLEEP_ALLOWED)
					CPRINTS("Disable console in deepsleep");
			}


			/* UART is not being used  */
			uart_ready_for_deepsleep = LOW_SPEED_DEEP_SLEEP_ALLOWED
						&& !uart_tx_in_progress()
						&& uart_buffer_empty();

			if (uart_ready_for_deepsleep) {

				idle_dsleep_cnt++;

				/*
				 * config UART Rx as GPIO wakeup interrupt
				 * source
				 */
				uart_enter_dsleep();

				prepare_for_deep_sleep();

				/*
				 * 'max_sleep_time' value should be big
				 * enough so that hibernation timer's interrupt
				 * triggers only after 'wfi' completes its
				 * excution.
				 */
				max_sleep_time -= (get_time().le.lo - t0.le.lo);

				/* setup/enable htimer wakeup interrupt */
				system_set_htimer_alarm(0, max_sleep_time);
			} else {
				idle_sleep_cnt++;
			}

			/* Wait for interrupt: goes into deep sleep. */
			asm("wfi");

			if (uart_ready_for_deepsleep) {

				resume_from_deep_sleep();

				/*
				 * Fast forward timer according to htimer
				 * counter:
				 * Since all blocks including timers will be in
				 * sleep mode, timers stops except hibernate
				 * timer.
				 * And system schedule timer should be corrected
				 * after wakeup by either hibernate timer or
				 * GPIO_UART_RX interrupt.
				 */
				ht_t1 = system_get_htimer();

				/* disable/clear htimer wakeup interrupt */
				system_reset_htimer_alarm();

				t1.val = t0.val +
				       (uint64_t)(max_sleep_time - ht_t1.le.lo);

				force_time(t1);

				/* re-eanble UART */
				uart_exit_dsleep();

				/* Record time spent in deep sleep. */
				total_idle_dsleep_time_us +=
				       (uint64_t)(max_sleep_time - ht_t1.le.lo);
			}

		} else { /* CPU 'Sleep' mode */

			idle_sleep_cnt++;

			asm("wfi");

		}

		interrupt_enable();
	} /* while(1) */
}
#endif /*CONFIG_LOW_POWER_IDLE*/

#ifdef CONFIG_LOW_POWER_IDLE
/**
 * Print low power idle statistics
 */
static int command_idle_stats(int argc, char **argv)
{
	timestamp_t ts = get_time();

	ccprintf("Num idle calls that sleep:           %d\n", idle_sleep_cnt);
	ccprintf("Num idle calls that deep-sleep:      %d\n", idle_dsleep_cnt);

	ccprintf("Total Time spent in deep-sleep(sec): %.6ld(s)\n",
						total_idle_dsleep_time_us);
	ccprintf("Total time on:                       %.6lds\n\n", ts.val);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(idlestats, command_idle_stats,
			"",
			"Print last idle stats",
			NULL);

/**
 * Configure deep sleep clock settings.
 */
static int command_dsleep(int argc, char **argv)
{
	int v;

	if (argc > 1) {
		if (parse_bool(argv[1], &v)) {
			/*
			 * Force deep sleep not to use heavy sleep mode or
			 * allow it to use the heavy sleep mode.
			 */
			if (v)  /* 'on' */
				disable_sleep(SLEEP_MASK_FORCE_NO_LOW_SPEED);
			else    /* 'off' */
				enable_sleep(SLEEP_MASK_FORCE_NO_LOW_SPEED);
		} else {
			/* Set console in use timeout. */
			char *e;

			v = strtoi(argv[1], &e, 10);
			if (*e)
				return EC_ERROR_PARAM1;

			console_in_use_timeout_sec = v;

			/* Refresh console in use to use new timeout. */
			clock_refresh_console_in_use();
		}
	}

	ccprintf("Sleep mask: %08x\n", sleep_mask);
	ccprintf("Console in use timeout:   %d sec\n",
			console_in_use_timeout_sec);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(dsleep, command_dsleep,
			"[ on | off | <timeout> sec]",
			"Deep sleep clock settings:\nUse 'on' to force deep \
			sleep NOT to enter heavysleep mode.\nUse 'off' to \
			allow deep sleep to use heavysleep whenever conditions \
			allow.\n \
			Give a timeout value for the console in use timeout.\n \
			See also 'sleepmask'.",
			NULL);
#endif /* CONFIG_LOW_POWER_IDLE */
