/* Copyright 2017 The Chromium OS Authors. All rights reserved.
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
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "shared_mem.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"
#include "vboot_hash.h"

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

static int freq = 48000000;

void clock_wait_cycles(uint32_t cycles)
{
	asm volatile("1: subs %0, #1\n"
		     "   bne 1b\n" : "+r"(cycles));
}

int clock_get_freq(void)
{
	return freq;
}

/** clock_init
 * @note
 * MEC17xx implements 4 control bits in the VBAT Clock Enable register.
 * It also implements an internal silicon 32KHz +/- 2% oscillator powered
 * by VBAT.
 * b[3] = XOSEL 0=parallel, 1=single-ended
 * b[2] = 32KHZ_SOURCE specifies source of always-on clock domain
 *        0=internal silicon oscillator
 *        1=crystal XOSEL pin(s)
 * b[1] = EXT_32K use always-on clock domain or external 32KHZ_IN pin
 *        0=32K source is always-on clock domain
 *        1=32K source is 32KHZ_IN pin (GPIO 0165)
 * b[0] = 32K_SUPPRESS
 *        0=32K clock domain stays enabled if VTR is off. Powered by VBAT
 *        1=32K clock domain is disabled if VTR is off.
 * Set b[3] based on CONFIG_CLOCK_CRYSTAL
 * Set b[2:0] = 100b
 *    b[0]=0 32K clock domain always on (requires VBAT if VTR is off)
 *    b[1]=0 32K source is the 32K clock domain NOT the 32KHZ_IN pin
 *    b[2]=1 If activity detected on crystal pins switch 32K input from
 *           internal silicon oscillator to XOSEL pin(s) based on b[3].
 */
void clock_init(void)
{
	int __attribute__((unused)) dummy;

	trace0(0, MEC, 0, "Clock Init");

#ifdef CONFIG_CLOCK_CRYSTAL
	/* XOSEL: 0 = Parallel resonant crystal */
	MEC17XX_VBAT_CE &= ~(1ul << 3);

#else
	/* XOSEL: 1 = Single ended clock source */
	MEC17XX_VBAT_CE |= (1ul << 3);
#endif

	/* 32K clock enable */
	MEC17XX_VBAT_CE = (MEC17XX_VBAT_CE & ~(0x03)) | (1ul << 2);

#ifdef CONFIG_CLOCK_CRYSTAL
	/* Wait for crystal to stabilize (OSC_LOCK == 1) */
	while (!(MEC17XX_PCR_CHIP_OSC_ID & 0x100))
		;
#endif
	trace0(0, MEC, 0, "PLL OSC is Locked");
#ifndef LFW
	dummy = shared_mem_size();
	trace11(0, MEC, 0, "Shared Memory size = 0x%08x", (uint32_t)dummy);
#endif
}

/**
 * Speed through boot + vboot hash calculation, dropping our processor clock
 * only after vboot hashing is completed.
 */
static void clock_turbo_disable(void);
DECLARE_DEFERRED(clock_turbo_disable);

static void clock_turbo_disable(void)
{
#ifdef CONFIG_VBOOT_HASH
	if (vboot_hash_in_progress())
		hook_call_deferred(&clock_turbo_disable_data, 100 * MSEC);
	else
#endif
		/* Use 12 MHz processor clock for power savings */
		MEC17XX_PCR_PROC_CLK_CTL = 4;
}
DECLARE_HOOK(HOOK_INIT, clock_turbo_disable, HOOK_PRIO_INIT_VBOOT_HASH + 1);

#ifdef CONFIG_LOW_POWER_IDLE
/**
 * initialization of Hibernation timer0
 * GIRQ=21, aggregator bit = 1, Direct NVIC = 112
 * NVIC direct connect interrupts are used for all peripherals
 * (exception GPIO's) then the MEC17XX_INT_BLK_EN GIRQ bit should not be
 * set.
 */
static void htimer_init(void)
{
	MEC17XX_INT_ENABLE(MEC17XX_HTIMER_GIRQ) = MEC17XX_HTIMER_GIRQ_BIT(0);
	MEC17XX_HTIMER_PRELOAD(0) = 0; /* disable at beginning */

	task_enable_irq(MEC17XX_IRQ_HTIMER0);
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
			/* 0.125(=1/8) per clock */
			MEC17XX_HTIMER_CONTROL(0) = 1;
			/* (number of counts to be loaded)
			 * = seconds * ( 8 clocks per second )
			 *   + microseconds / 125000
			 *   ---> (0 if (microseconds < 125000)
			 */
			MEC17XX_HTIMER_PRELOAD(0) =
				(seconds * 8 + microseconds / 125000);

		} else { /* count up to 2 sec. */

			MEC17XX_HTIMER_CONTROL(0) = 0; /* 30.5(= 2/61) usec */

			/* (number of counts to be loaded)
			 * = (total microseconds) / 30.5;
			 */
			MEC17XX_HTIMER_PRELOAD(0) =
				(seconds * 1000000 + microseconds) * 2 / 61;
		}
	}
}

/**
 * return time slept in micro-seconds
 */
static timestamp_t system_get_htimer(void)
{
	uint16_t count;
	timestamp_t time;

	count =  MEC17XX_HTIMER_COUNT(0);


	if (MEC17XX_HTIMER_CONTROL(0) == 1) /* if > 2 sec */
		/* 0.125 sec per count */
		time.le.lo = (uint32_t)(count * 125000);
	else    /* if < 2 sec */
		/* 30.5(=61/2)usec per count */
		time.le.lo = (uint32_t)(count * 61 / 2);

	time.le.hi = 0;

	return time;  /* in uSec */
}

/**
 * Disable and clear hibernation timer interrupt
 */
static void system_reset_htimer_alarm(void)
{
	MEC17XX_HTIMER_PRELOAD(0) = 0;
	MEC17XX_INT_SOURCE(MEC17XX_HTIMER_GIRQ) = MEC17XX_HTIMER_GIRQ_BIT(0);
}


/**
 * This is mec17xx specific and equivalent to ARM Cortex's
 * 'DeepSleep' via system control block register, CPU_SCB_SYSCTRL
 * MEC17xx has new SLP_ALL feature.
 * When SLP_ALL is enabled and HW sees sleep entry trigger from CPU.
 * 1. HW saves PCR.SLP_EN registers
 * 2. HW sets all PCR.SLP_EN bits to 1.
 * 3. System sleeps
 * 4. wake event wakes system
 * 5. HW restores original values of all PCR.SLP_EN registers
 * Only manually set PCR.SLP_EN bits to 1 for blocks not being used.
 *
 */
static void prepare_for_deep_sleep(void)
{
	trace0(0, MEC, 0, "Prepare for Deep Sleep");

	/* sysTick timer */
	CPU_NVIC_ST_CTRL &= ~ST_ENABLE;
	CPU_NVIC_ST_CTRL &= ~ST_COUNTFLAG;

#ifdef CONFIG_CHIPSET_DEBUG
	/* Disable JTAG and preserve mode */
	MEC17XX_EC_JTAG_EN &= ~(MEC17XX_JTAG_ENABLE);
#endif

	/*
	 * Clear ADC activate bit. If a conversion is in progress the
	 * ADC block will not enter low power until the converstion is
	 * complete.
	 */
	MEC17XX_ADC_CTRL &= ~1;

	/* Stop watchdog */
	MEC17XX_WDG_CTL &= ~1;

	/* Stop timers */
	MEC17XX_TMR32_CTL(0) &= ~1;
	MEC17XX_TMR32_CTL(1) &= ~1;
	MEC17XX_TMR16_CTL(0) &= ~1;

	/*
	 * Clear SLP_EN bit(s) for wake sources.
	 * Currently only Hibernation timer 0.
	 * GPIO pins can always wake.
	 */
	MEC17XX_PCR_SLP_EN3 &= ~(MEC17XX_PCR_SLP_EN3_HTMR0);

#ifdef CONFIG_PWM
	pwm_keep_awake();    /* clear sleep enables of active PWM's */
#else
	/* Disable 100 Khz clock */
	MEC17XX_PCR_SLOW_CLK_CTL &= 0xFFFFFC00;
#endif

#ifndef CONFIG_POWER_S0IX
#ifdef CONFIG_ESPI
	MEC17XX_ESPI_ACTIVATE &= ~1;
#else
	MEC17XX_LPC_ACT = 0x0;
#endif
#endif

	MEC17XX_PCR_SYS_SLP_CTL |= MEC17XX_PCR_SYS_SLP_HEAVY;
	MEC17XX_PCR_SYS_SLP_CTL |= MEC17XX_PCR_SYS_SLP_ALL;

	CPU_NVIC_ST_CTRL &= ~ST_TICKINT; /* SYS_TICK_INT_DISABLE */

	/* Enable DeepSleep for core */
	CPU_SCB_SYSCTRL |= (1 << 2);
}

static void resume_from_deep_sleep(void)
{
	CPU_SCB_SYSCTRL &= ~(1 << 2);

#ifdef CONFIG_CHIPSET_DEBUG
	MEC17XX_EC_JTAG_EN |= (MEC17XX_JTAG_ENABLE);
#endif
	MEC17XX_ADC_CTRL |= 1;

	/* Enable timer */
	MEC17XX_TMR32_CTL(0) |= 1;
	MEC17XX_TMR32_CTL(1) |= 1;
	MEC17XX_TMR16_CTL(0) |= 1;

	/* Enable watchdog */
#ifdef CONFIG_CHIPSET_DEBUG
	/* enable WDG stall on active JTAG */
	MEC17XX_WDG_CTL |= (1 << 4) + (1 << 0);
#else
	MEC17XX_WDG_CTL |= 1;
#endif

	MEC17XX_PCR_SLOW_CLK_CTL |= 0x1e0;

	/*
	 * re-enable hibernation timer 0 PCR.SLP_EN to
	 * reduce power.
	 */
	MEC17XX_PCR_SLP_EN3 |= (MEC17XX_PCR_SLP_EN3_HTMR0);

	MEC17XX_PCR_SYS_SLP_CTL = 0x00;  /* default */

#ifndef CONFIG_POWER_S0IX
#ifdef CONFIG_ESPI
	MEC17XX_ESPI_ACTIVATE |= 1; /* Enable eSPI */
#else
	MEC17XX_LPC_ACT |= 1; /* Enable LPC */
#endif
#endif
}


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
	CPRINTS("MEC1701 low power idle task started");

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
			trace0(0, MEC, 0, "Enough time for Deep Sleep");
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
					CPRINTS("MEC1701 Disable console "
						"in deepsleep");
			}


			/* UART is not being used  */
			uart_ready_for_deepsleep = LOW_SPEED_DEEP_SLEEP_ALLOWED
						&& !uart_tx_in_progress()
						&& uart_buffer_empty();

			/*
			 * Since MEC17XX's heavysleep modes requires all block
			 * to be sleepable, UART/console's readiness is final
			 * decision factor of heavysleep of EC.
			 */
			if (uart_ready_for_deepsleep) {

				idle_dsleep_cnt++;

				/*
				 * config UART Rx as GPIO wakeup interrupt
				 * source
				 */
				uart_enter_dsleep();

				/* MEC17XX specific deep-sleep mode */
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

#ifdef CONFIG_CMD_IDLE_STATS
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
			"Print last idle stats");
#endif /* defined(CONFIG_CMD_IDLE_STATS) */

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
			"Deep sleep clock settings:\nUse 'on' to force deep "
			"sleep NOT to enter heavysleep mode.\nUse 'off' to "
			"allow deepsleep to use heavysleep whenever conditions "
			"allow.\n"
			"Give a timeout value for the console in use timeout.\n"
			"See also 'sleepmask'.");
#endif /* CONFIG_LOW_POWER_IDLE */
