/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks and power management settings */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "registers.h"
#include "util.h"
#include "cpu.h"
#include "hwtimer.h"
#include "shared_mem.h"
#include "system.h"
#include "task.h"
#include "util.h"
#include "timer.h"
#include "uart.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CLOCK, outstr)
#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ## args)

#ifdef CONFIG_LOW_POWER_IDLE
/* Recovery time for HvySlp2 is 0 usec */
#define HEAVY_SLEEP_RECOVER_TIME_USEC   75

/* time A(until final 'max_sleep_time' is recalculate, 40usec)
 * + time B(until 'wfi' is excuted. ?)
 */
#define SET_HTIMER_DELAY_USEC           200

static int idle_sleep_cnt;
static int idle_dsleep_cnt;
static uint64_t total_idle_dsleep_time_us;

/*
 * Fixed amount of time to keep the console in use flag true after boot in
 * order to give a permanent window in which the low speed clock is not used.
 */
#define CONSOLE_IN_USE_ON_BOOT_TIME (15*SECOND)
static int console_in_use_timeout_sec = 60;
static timestamp_t console_expire_time;
#endif /*CONFIG_LOW_POWER_IDLE */

static int freq = 48000000;

void clock_wait_cycles(uint32_t cycles)
{
	asm("1: subs %0, #1\n"
	    "   bne 1b\n" :: "r"(cycles));
}

int clock_get_freq(void)
{
	return freq;
}

void clock_init(void)
{
#ifdef CONFIG_CLOCK_CRYSTAL
	/* XOSEL: 0 = Parallel resonant crystal */
	MEC1322_VBAT_CE &= ~0x1;
#else
	/* XOSEL: 1 = Single ended clock source */
	MEC1322_VBAT_CE |= 0x1;
#endif

	/* 32K clock enable */
	MEC1322_VBAT_CE |= 0x2;

#ifdef CONFIG_CLOCK_CRYSTAL
	/* Wait for crystal to stabilize (OSC_LOCK == 1) */
	while (!(MEC1322_PCR_CHIP_OSC_ID & 0x100))
		;
#endif
}

#ifdef CONFIG_LOW_POWER_IDLE
/*
 * initialization of Hibernation timer
 */
static void htimer_init(void)
{
	MEC1322_INT_BLK_EN |= 1 << 17;
	MEC1322_INT_ENABLE(17) |= 1 << 20;  /* GIRQ=17, aggregator bit = 20 */
	MEC1322_HTIMER_PRELOAD = 0;  /* disable at begining */

	task_enable_irq(MEC1322_IRQ_HTIMER);
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
	if (seconds || microseconds) {

		if (seconds > 2) {
			/* count from 2 sec to 2 hrs, mec1322 sec 18.10.2 */
			ASSERT(seconds <= 0xffff / 8);
			MEC1322_HTIMER_CONTROL = 1; /* 0.125(=1/8) per clock */
			/* (number of counts to be loaded)
			 * = seconds * ( 8 clocks per second )
			 *   + microseconds / 125000
			 *   ---> (0 if (microseconds < 125000)
			 */
			MEC1322_HTIMER_PRELOAD =
				(seconds * 8 + microseconds / 125000);

		} else { /* count up to 2 sec. */

			MEC1322_HTIMER_CONTROL = 0; /* 30.5(= 2/71) usec */

			/* (number of counts to be loaded)
			 * = (total microseconds) / 30.5;
			 */
			MEC1322_HTIMER_PRELOAD =
				(seconds * 1000000 + microseconds) * 2 / 71;
		}
	}
}

/*
 * return time slept in micro-seconds
 */
static timestamp_t system_get_htimer(void)
{
	uint16_t count;
	timestamp_t time;

	count =  MEC1322_HTIMER_COUNT;


	if (MEC1322_HTIMER_CONTROL == 1) /* if > 2 sec */
		/* 0.125 sec per count */
		time.le.lo = (uint32_t)(count * 125000);
	else    /* if < 2 sec */
		/* 30.5(=71/2)usec per count */
		time.le.lo = (uint32_t)(count * 71 / 2);

	time.le.hi = 0;

	return time;  /* in uSec */
}

/**
 * Disable and clear hibernation timer interrupt
 */
static void system_reset_htimer_alarm(void)
{
	/* GIRQ=17, aggregator bit = 20 */
	/* MEC1322_INT_BLK_DIS |= 1 << 17; */
	/* MEC1322_INT_DISABLE(17) |= 1 << 20;*/
	MEC1322_HTIMER_PRELOAD = 0;
}

/*
 * This is mec1322 specific and equivalent to ARM Cortex's
 * 'DeepSlee' via system control block register, CPU_SCB_SYSCTRL
 */
static void prepare_for_deep_sleep(void)
{
	/* sysTick timer */
	CPU_NVIC_ST_CTRL &= ~ST_ENABLE;
	CPU_NVIC_ST_CTRL &= ~ST_COUNTFLAG;

	/* Disable 32KHz clock */
	MEC1322_VBAT_CE &= ~0x2;

	/* Disable JTAG */
	MEC1322_EC_JTAG_EN &= ~1;
	/* Power down ADC VREF, ADC_VREF override ADC_CTRL */
	MEC1322_EC_ADC_VREF_PD |= 1;

	/* Stop watchdog */
	MEC1322_WDG_CTL &= ~1;

	/* Stop timers */
	MEC1322_TMR32_CTL(0) &= ~1;
	MEC1322_TMR32_CTL(1) &= ~1;
	MEC1322_TMR16_CTL(0) &= ~1;

	MEC1322_PCR_CHIP_SLP_EN |= 0x3;
	MEC1322_PCR_EC_SLP_EN = 0xFFFFFFFF;
	MEC1322_PCR_HOST_SLP_EN = 0xFFFFFFFF;
	MEC1322_PCR_EC_SLP_EN2 = 0xFFFFFFFF;

	MEC1322_LPC_ACT = 0x0;
	MEC1322_LPC_CLK_CTRL |= 0x2;

	MEC1322_PCR_SLOW_CLK_CTL &= 0xFFFFFC00;

	MEC1322_PCR_SYS_SLP_CTL = 0x2;  /* heavysleep 2 */

	CPU_NVIC_ST_CTRL &= ~ST_TICKINT; /* SYS_TICK_INT_DISABLE */
}

static void resume_from_deep_sleep(void)
{
	CPU_NVIC_ST_CTRL |= ST_TICKINT; /* SYS_TICK_INT_ENABLE */
	CPU_NVIC_ST_CTRL |= ST_ENABLE;

	MEC1322_EC_JTAG_EN = 1;
	MEC1322_EC_ADC_VREF_PD &= ~1;
	/* ADC_VREF override ADC_CTRL ! */
	/* MEC1322_ADC_CTRL |= 1 << 0; */

	/* Enable timer */
	MEC1322_TMR32_CTL(0) |= 1;
	MEC1322_TMR32_CTL(1) |= 1;
	MEC1322_TMR16_CTL(0) |= 1;

	/* Enable watchdog */
	MEC1322_WDG_CTL |= 1;

	/* Enable 32KHz clock */
	MEC1322_VBAT_CE |= 0x2;

	MEC1322_PCR_SLOW_CLK_CTL |= 0x1e0;
	MEC1322_PCR_CHIP_SLP_EN &= ~0x3;
	MEC1322_PCR_EC_SLP_EN &= ~0xe0700ff7;
	MEC1322_PCR_HOST_SLP_EN &= ~0x5f003;
	MEC1322_PCR_EC_SLP_EN2 &= ~0x1ffffff8;

	MEC1322_PCR_SYS_SLP_CTL = 0xF8;  /* default */

	/* Enable UART */
	MEC1322_LPC_ACT |= 1;
	MEC1322_LPC_CLK_CTRL &= ~0x2;

	MEC1322_PCR_SLOW_CLK_CTL = 0x1E0;
}


void clock_refresh_console_in_use(void)
{
	disable_sleep(SLEEP_MASK_CONSOLE);

	/* Set console in use expire time. */
	console_expire_time = get_time();
	console_expire_time.val += console_in_use_timeout_sec * SECOND;
}

/* Low power idle task.  Executed when no tasks are ready to be scheduled. */
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

			idle_dsleep_cnt++;

			/*
			 * Check if the console use has expired and console
			 * sleep is masked by GPIO(UART-RX) interrupt.
			 * */
			if ((sleep_mask & SLEEP_MASK_CONSOLE) &&
					t0.val > console_expire_time.val) {
				/* Enable low speed deep sleep. */
				enable_sleep(SLEEP_MASK_CONSOLE);

				/*
				 * Wait one clock before checking if low speed
				 * deep sleep is allowed to give time for
				 * sleep mask to update.
				 */
				clock_wait_cycles(1);

				if (LOW_SPEED_DEEP_SLEEP_ALLOWED)
					CPRINTS("Disable console in deepsleep");
			}


			/* UART is not being used  */
			uart_ready_for_deepsleep = LOW_SPEED_DEEP_SLEEP_ALLOWED
						&& !uart_tx_in_progress()
						&& uart_buffer_empty();

			/* config UART Rx as GPIO wakeup interrupt source */
			if (uart_ready_for_deepsleep) {
				uart_enter_dsleep();
			/*
			 * Since MEC1322's heavysleep modes requires all block
			 * to be sleepable, UART/console's readiness is decision
			 * factor of realy heavysleep of EC.
			 * */
				/* MEC1322 specific deep-sleep mode:*/
				prepare_for_deep_sleep();

				/* time-A: 'max_sleep_value' value should be big
				 * enough so that hibernation timer's interrupt
				 * trigger only after 'wfi' completes its
				 * excution.
				 */
				max_sleep_time -= (get_time().le.lo - t0.le.lo);

				/* setup/enable htimer wakeup interrupt */
				system_set_htimer_alarm(0, max_sleep_time);
			}

			/* Wait for interrupt: goes into deep sleep. */
			asm("wfi"); /* time-B */

			if (uart_ready_for_deepsleep) {

				resume_from_deep_sleep();

			/*
			 * Since all blocks including timers will be in sleep
			 * mode, timers stops except hibernate timer.
			 * And system schedule timer should be corrected after
			 * wakeup by either hibernate timer or GPIO_UART_RX.
			 * --> Fast forward timer according to htimer counter.
			 * 1. read hibernation timer value, ht_t1.
			 * 2. check if (ht_t1 == 0), htimer is expired and is
			 *    wakeup source.
			 * 3. else (if ht_t1 != 0), himter is NOT wakeup source.
			 *    (UART_RX is wakeup soruce)
			 * 4. for case, timer correction value(delta) should be
			 *    (next_delay-HEAVY_SLEEP_RECOVER_TIME_USEC)-ht_t0
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

	ccprintf("Total Time spent in deep-sleep(sec): %.6ld(s), %lu(usec)\n",
						total_idle_dsleep_time_us,
						total_idle_dsleep_time_us);
	ccprintf("Total time on:                       %.6lds\n\n", ts.val);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(idlestats, command_idle_stats,
			"",
			"Print last idle stats",
			NULL);

#endif /* CONFIG_LOW_POWER_IDLE */
