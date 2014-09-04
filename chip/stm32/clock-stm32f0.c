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
#define RTC_TO_SEC(rt) ( \
	((((rt) & 0x300000) >> 20) * 10 + (((rt) & 0xf0000) >> 16)) * 3600 +\
	((((rt) & 0x7000) >> 12) * 10 + (((rt) & 0xf00) >> 8)) * 60 +\
	(((rt) & 0x70) >> 4) * 10 + ((rt) & 0xf))
#define SEC_TO_RTC(s) ( \
	(((s) / 36000) << 20) | ((((s) / 3600) % 10) << 16) |\
	((((s) % 3600) / 600) << 12) | ((((s) % 600) / 60) << 8) |\
	((((s) % 60) / 10) << 4) | ((s) % 10))

uint32_t set_rtc_alarm(unsigned delay_s, unsigned delay_us)
{
	uint32_t rtc, rtcss;
	uint32_t alarm_sec, alarm_us;

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

	ccprintf("isr: 0x%08x, rtc: 0x%08x, rtcss: 0x%08x\n", STM32_RTC_ISR, rtc, rtcss);

	/* TODO: need to check input args, must be less than 24 hours */
	/* Calculate alarm time */
	alarm_sec = RTC_TO_SEC(rtc) + delay_s;
	alarm_us = (RTC_PREDIV_S - rtcss) * US_PER_RTC_TICK + delay_us;
	alarm_sec += alarm_us / 1000000;
	alarm_sec %= 86400;
	alarm_us = alarm_us % 1000000;

	ccprintf("rtc(s): %d, rtc(us): %d\n", RTC_TO_SEC(rtc), (RTC_PREDIV_S - rtcss) * US_PER_RTC_TICK);
	ccprintf("alarm_sec: %d, alarm_us: %d\n", alarm_sec, alarm_us);

	STM32_RTC_ALRMAR = SEC_TO_RTC(alarm_sec);
	STM32_RTC_ALRMASSR = RTC_PREDIV_S - (alarm_us / US_PER_RTC_TICK);
	//STM32_RTC_ALRMAR = 0;
	//STM32_RTC_ALRMASSR = 0x11;

	/* Check for match on hours, minutes, seconds, and subsecond */
	STM32_RTC_ALRMAR |= 0x808080;
	STM32_RTC_ALRMASSR |= 0x0f000000;

	ccprintf("alarm ar: %08x, assr: %08x\n", STM32_RTC_ALRMAR, STM32_RTC_ALRMASSR);

	/* Enable alarm and alarm interrupt */
	STM32_EXTI_PR = (1 << 17);
	STM32_EXTI_IMR |= (1 << 17);
	STM32_RTC_CR |= STM32_RTC_CR_ALRAE;

	/* TODO: what to return? */
	return 0;
}

uint32_t reset_rtc_alarm(void)
{
	/* Disable alarm */
	STM32_RTC_CR &= ~STM32_RTC_CR_ALRAE;
	STM32_RTC_ISR &= ~STM32_RTC_ISR_ALRAF;

	/* Disable RTC alarm interrupt */
	STM32_EXTI_IMR &= (1 << 17);
	STM32_EXTI_PR = (1 << 17);

	/* TODO: what to return? */
	return 0;
}

void __rtc_alarm_irq(void)
{
	ccprintf("alarm\n");
	reset_rtc_alarm();
}
DECLARE_IRQ(STM32_IRQ_RTC_WAKEUP, __rtc_alarm_irq, 1);

static int alarm_test(int argc, char **argv)
{
	char *e;
	int s = 1, us = 0;

	if (argc > 2) {
		s = strtoi(argv[1], &e, 10);
		if (*e)
			return EC_ERROR_PARAM1;

		us = strtoi(argv[2], &e, 10);
		if (*e)
			return EC_ERROR_PARAM2;
	}

	set_rtc_alarm(s, us);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(alarm, alarm_test,
			"",
			"",
			NULL);

int clock_get_freq(void)
{
	return CPU_CLOCK;
}

void clock_enable_module(enum module_id module, int enable)
{
}

void clock_init(void)
{
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
	STM32_RTC_PRER = (1 << 16) | 19999;

	/* Start RTC timer */
	STM32_RTC_ISR &= ~STM32_RTC_ISR_INIT;
	while (STM32_RTC_ISR & STM32_RTC_ISR_INITF)
		;

	/* Enable RTC alarm interrupt */
	STM32_RTC_CR |= STM32_RTC_CR_ALRAIE;
	STM32_EXTI_RTSR |= (1 << 17);
	task_enable_irq(STM32_IRQ_RTC_WAKEUP);

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
}
