/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks and power management settings */

#include "chipset.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* use 48Mhz USB-synchronized High-speed oscillator */
#define HSI48_CLOCK 48000000

/* use PLL at 38.4MHz as system clock. */
#define PLL_CLOCK 38400000

/*
 * RTC clock frequency (connected to LSI clock)
 *
 * TODO(crosbug.com/p/12281): Calibrate LSI frequency on a per-chip basis.  The
 * LSI on any given chip can be between 30 kHz to 60 kHz.  Without calibration,
 * LSI frequency may be off by as much as 50%.  Fortunately, we don't do any
 * high-precision delays based solely on LSI.
 */
#define RTC_FREQ (40000 / 2) /* Hz */
#define RTC_PREDIV_S (RTC_FREQ - 1)
#define US_PER_RTC_TICK (1000000 / RTC_FREQ)

/* Convert between RTC regs in BCD and seconds */
static inline uint32_t rtc_to_sec(uint32_t rtc)
{
	uint32_t sec;

	/* convert the hours field */
	sec = (((rtc & 0x300000) >> 20) * 10 + ((rtc & 0xf0000) >> 16)) * 3600;
	/* convert the minutes field */
	sec += (((rtc & 0x7000) >> 12) * 10 + ((rtc & 0xf00) >> 8)) * 60;
	/* convert the seconds field */
	sec += ((rtc & 0x70) >> 4) * 10 + (rtc & 0xf);

	return sec;
}
static inline uint32_t sec_to_rtc(uint32_t sec)
{
	uint32_t rtc;

	/* convert the hours field */
	rtc = ((sec / 36000) << 20) | (((sec / 3600) % 10) << 16);
	/* convert the minutes field */
	rtc |= (((sec % 3600) / 600) << 12) | (((sec % 600) / 60) << 8);
	/* convert the seconds field */
	rtc |= (((sec % 60) / 10) << 4) | (sec % 10);

	return rtc;
}

uint32_t set_rtc_alarm(unsigned delay_s, unsigned delay_us)
{
	uint32_t rtc, rtcss;
	uint64_t rtc_us;
	uint32_t alarm_sec, alarm_us;

	/* Alarm must be within 1 day (86400 seconds) */
	ASSERT((delay_s + delay_us / SECOND) < 86400);

	/* Make sure alarm is disabled */
	STM32_RTC_CR &= ~STM32_RTC_CR_ALRAE;
	while (!(STM32_RTC_ISR & STM32_RTC_ISR_ALRAWF))
		;
	STM32_RTC_ISR &= ~STM32_RTC_ISR_ALRAF;

	/* Read current time */
	rtc = STM32_RTC_TR;
	rtcss = STM32_RTC_SSR;
	/*
	 * Dummy read of date register necessary after reading other two regs
	 * because value of other regs is locked until after DR is read to
	 * avoid synchronization issues.
	 */
	(void)STM32_RTC_DR;

	/* Calculate alarm time */
	alarm_sec = rtc_to_sec(rtc);
	alarm_us = (RTC_PREDIV_S - rtcss) * US_PER_RTC_TICK;
	rtc_us = (uint64_t)alarm_sec * SECOND + alarm_us;
	alarm_us += delay_us;
	alarm_sec = (alarm_sec + delay_s + alarm_us / SECOND) % 86400;
	alarm_us = alarm_us % 1000000;

	/* Set alarm time */
	STM32_RTC_ALRMAR = sec_to_rtc(alarm_sec);
	STM32_RTC_ALRMASSR = RTC_PREDIV_S - (alarm_us / US_PER_RTC_TICK);
	/* Check for match on hours, minutes, seconds, and subsecond */
	STM32_RTC_ALRMAR |= 0xc0000000;
	STM32_RTC_ALRMASSR |= 0x0f000000;

	/* Enable alarm and alarm interrupt */
	STM32_EXTI_PR = (1 << 17);
	STM32_EXTI_IMR |= (1 << 17);
	STM32_RTC_CR |= STM32_RTC_CR_ALRAE;

	/* Return current time */
	return rtc_us;
}

uint32_t reset_rtc_alarm(void)
{
	uint32_t rtc, rtcss;

	/* Disable alarm */
	STM32_RTC_CR &= ~STM32_RTC_CR_ALRAE;
	STM32_RTC_ISR &= ~STM32_RTC_ISR_ALRAF;

	/* Disable RTC alarm interrupt */
	STM32_EXTI_IMR &= (1 << 17);
	STM32_EXTI_PR = (1 << 17);

	/* Read current time */
	rtc = STM32_RTC_TR;
	rtcss = STM32_RTC_SSR;
	/*
	 * Dummy read of date register necessary after reading other two regs
	 * because value of other regs is locked until after DR is read to
	 * avoid synchronization issues.
	 */
	(void)STM32_RTC_DR;

	/* Return current time */
	return (uint64_t)rtc_to_sec(rtc) * SECOND +
		(RTC_PREDIV_S - rtcss) * US_PER_RTC_TICK;
}

void __rtc_alarm_irq(void)
{
	reset_rtc_alarm();
}
DECLARE_IRQ(STM32_IRQ_RTC_WAKEUP, __rtc_alarm_irq, 1);

int clock_get_freq(void)
{
	return CPU_CLOCK;
}

void clock_enable_module(enum module_id module, int enable)
{
}

void clock_init(void)
{
	/*
	 * The initial state :
	 *  SYSCLK from HSI (=8MHz), no divider on AHB, APB1, APB2
	 *  PLL unlocked, RTC enabled on LSE
	 */

	/* put 1 Wait-State for flash access to ensure proper reads at 48Mhz */
	STM32_FLASH_ACR = 0x1001; /* 1 WS / Prefetch enabled */

	/* Ensure that HSI48 is ON */
	if (!(STM32_RCC_CR2 & (1 << 17))) {
		/* Enable HSI */
		STM32_RCC_CR2 |= 1 << 16;
		/* Wait for HSI to be ready */
		while (!(STM32_RCC_CR2 & (1 << 17)))
			;
	}

#if (CPU_CLOCK == HSI48_CLOCK)
	/*
	 * HSI48 = 48MHz, no prescaler, no MCO, no PLL
	 * therefore PCLK = FCLK = SYSCLK = 48MHz
	 * USB uses HSI48 = 48MHz
	 */

	/* switch SYSCLK to HSI48 */
	STM32_RCC_CFGR = 0x00000003;

	/* wait until the HSI48 is the clock source */
	while ((STM32_RCC_CFGR & 0xc) != 0xc)
		;

#elif (CPU_CLOCK == PLL_CLOCK)
	/*
	 * HSI48 = 48MHz, no prescalar, no MCO, with PLL *4/5 => 38.4MHz SYSCLK
	 * therefore PCLK = FCLK = SYSCLK = 38.4MHz
	 * USB uses HSI48 = 48MHz
	 */

	/* If PLL is the clock source, PLL has already been set up. */
	if ((STM32_RCC_CFGR & 0xc) == 0x8)
		return;

	/*
	 * Specify HSI48 clock as input clock to PLL and set PLL multiplier
	 * and divider.
	 */
	STM32_RCC_CFGR = 0x00098000;
	STM32_RCC_CFGR2 = 0x4;

	/* Enable the PLL. */
	STM32_RCC_CR |= 0x01000000;

	/* Wait until PLL is ready. */
	while (!(STM32_RCC_CR & 0x02000000))
		;

	/* Switch SYSCLK to PLL. */
	STM32_RCC_CFGR |= 0x2;

	/* wait until the PLL is the clock source */
	while ((STM32_RCC_CFGR & 0xc) != 0x8)
		;

#else
#error "CPU_CLOCK must be either 48MHz or 38.4MHz"
#endif

	/* Unlock RTC write access */
	STM32_RTC_WPR = 0xca;
	STM32_RTC_WPR = 0x53;

	/* Enter RTC initialize mode */
	STM32_RTC_ISR |= STM32_RTC_ISR_INIT;
	while (!(STM32_RTC_ISR & STM32_RTC_ISR_INITF))
		STM32_RTC_ISR |= STM32_RTC_ISR_INIT;

	/*
	 * Set asynchronous clock freq to HSI/2 (20kHz) to maximize subsecond
	 * resolution. Set synchronous clock to 1 Hz.
	 */
	STM32_RTC_PRER = (1 << 16) | RTC_PREDIV_S;

	/* Start RTC timer */
	STM32_RTC_ISR &= ~STM32_RTC_ISR_INIT;
	while (STM32_RTC_ISR & STM32_RTC_ISR_INITF)
		;

	/* Enable RTC alarm interrupt */
	STM32_RTC_CR |= STM32_RTC_CR_ALRAIE;
	STM32_EXTI_RTSR |= (1 << 17);
	task_enable_irq(STM32_IRQ_RTC_WAKEUP);
}

/*****************************************************************************/
/* Console commands */

#ifdef CONFIG_CMD_RTC_ALARM
static int command_rtc_alarm_test(int argc, char **argv)
{
	int s = 1, us = 0;
	char *e;

	ccprintf("Setting RTC alarm\n");

	if (argc > 1) {
		s = strtoi(argv[1], &e, 10);
		if (*e)
			return EC_ERROR_PARAM1;

	}
	if (argc > 2) {
		us = strtoi(argv[2], &e, 10);
		if (*e)
			return EC_ERROR_PARAM2;

	}

	set_rtc_alarm(s, us);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rtc_alarm, command_rtc_alarm_test,
			"[seconds [microseconds]]",
			"Test alarm",
			NULL);
#endif /* CONFIG_CMD_RTC_ALARM */

