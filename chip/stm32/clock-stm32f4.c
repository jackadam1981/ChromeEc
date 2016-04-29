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
#include "hwtimer.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CLOCK, outstr)
#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ## args)

#ifdef HSE_CLOCK
#define RTC_PREDIV_A 39 
#define RTC_FREQ ((STM32F4_RTC_REQ) / (RTC_PREDIV_A + 1)) /* Hz */
#else
/* LSI clock is 40kHz-ish */
#define RTC_PREDIV_A 1
#define RTC_FREQ (40000 / (RTC_PREDIV_A + 1)) /* Hz */
#endif
#define RTC_PREDIV_S (RTC_FREQ - 1)
#define US_PER_RTC_TICK (1000000 / RTC_FREQ)

/* Lock and unlock RTC write access */
static inline void rtc_lock_regs(void)
{
	STM32_RTC_WPR = 0xff;
}
static inline void rtc_unlock_regs(void)
{
	STM32_RTC_WPR = 0xca;
	STM32_RTC_WPR = 0x53;
}

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

/* Return time diff between two rtc readings */
int32_t get_rtc_diff(uint32_t rtc0, uint32_t rtc0ss,
		     uint32_t rtc1, uint32_t rtc1ss)
{
	int32_t diff;

	/* Note: this only looks at the diff mod 10 seconds */
	diff =  ((rtc1 & 0xf) * SECOND +
		 (RTC_PREDIV_S - rtc1ss) * US_PER_RTC_TICK) -
		((rtc0 & 0xf) * SECOND +
		 (RTC_PREDIV_S - rtc0ss) * US_PER_RTC_TICK);

	return (diff < 0) ? (diff + 10*SECOND) : diff;
}

static inline void rtc_read(uint32_t *rtc, uint32_t *rtcss)
{
	/* Read current time synchronously */
	do {
		*rtc = STM32_RTC_TR;
		/*
		 * RTC_SSR must be read twice with identical values because
		 * glitches may occur for reads close to the RTCCLK edge.
		 */
		do {
			*rtcss = STM32_RTC_SSR;
		} while (*rtcss != STM32_RTC_SSR);
	} while (*rtc != STM32_RTC_TR);
}

void set_rtc_alarm(uint32_t delay_s, uint32_t delay_us,
		      uint32_t *rtc, uint32_t *rtcss)
{
	uint32_t alarm_sec, alarm_us;

	/* Alarm must be within 1 day (86400 seconds) */
	ASSERT((delay_s + delay_us / SECOND) < 86400);

	rtc_unlock_regs();

	/* Make sure alarm is disabled */
	STM32_RTC_CR &= ~STM32_RTC_CR_ALRAE;
	while (!(STM32_RTC_ISR & STM32_RTC_ISR_ALRAWF))
		;
	STM32_RTC_ISR &= ~STM32_RTC_ISR_ALRAF;

	rtc_read(rtc, rtcss);

	/* Calculate alarm time */
	alarm_sec = rtc_to_sec(*rtc) + delay_s;
	alarm_us = (RTC_PREDIV_S - *rtcss) * US_PER_RTC_TICK + delay_us;
 
	alarm_us = alarm_us % 1000000;
	/*
	 * If seconds is greater than 1 day, subtract by 1 day to deal with
	 * 24-hour rollover.
	 */
	if (alarm_sec >= 86400)
		alarm_sec -= 86400;

	/* Set alarm time */
	STM32_RTC_ALRMAR = sec_to_rtc(alarm_sec);
	STM32_RTC_ALRMASSR = RTC_PREDIV_S - (alarm_us / US_PER_RTC_TICK);
	/* Check for match on hours, minutes, seconds, and subsecond */
	STM32_RTC_ALRMAR |= 0xc0000000;
	STM32_RTC_ALRMASSR |= 0x0f000000;

	/* Enable alarm and alarm interrupt */
	STM32_EXTI_PR = EXTI_RTC_ALR_EVENT;
	STM32_EXTI_IMR |= EXTI_RTC_ALR_EVENT;
	STM32_RTC_CR |= STM32_RTC_CR_ALRAE;

	rtc_lock_regs();
}

void reset_rtc_alarm(uint32_t *rtc, uint32_t *rtcss)
{
	rtc_unlock_regs();

	/* Disable alarm */
	STM32_RTC_CR &= ~STM32_RTC_CR_ALRAE;
	STM32_RTC_ISR &= ~STM32_RTC_ISR_ALRAF;

	/* Disable RTC alarm interrupt */
	STM32_EXTI_IMR &= ~EXTI_RTC_ALR_EVENT;
	STM32_EXTI_PR = EXTI_RTC_ALR_EVENT;

	/* Read current time */
	rtc_read(rtc, rtcss);

	rtc_lock_regs();
}

void __rtc_alarm_irq(void)
{
	uint32_t rtc, rtcss;

	reset_rtc_alarm(&rtc, &rtcss);
}
DECLARE_IRQ(STM32_IRQ_RTC_ALARM, __rtc_alarm_irq, 1);

static void config_hispeed_clock(void)
{
#ifdef CONFIG_CLOCK_HSE
	int srcclock = HSE_CLOCK;
	int clk_check_mask = STM32_RCC_CR_HSERDY;
	int clk_enable_mask = STM32_RCC_CR_HSEON;
#else 
	int srcclock = STM32F4_HSI_CLOCK;
	int clk_check_mask = STM32_RCC_CR_HSIRDY;
	int clk_enable_mask = STM32_RCC_CR_HSION;
#endif
	int plldiv, pllinputclock;
	int pllmult, vcoclock;
	int systemdivq, systemclock;
	int usbdiv;
	int i2sdiv;

	int ahbpre, apb1pre, apb2pre;
	int rtcdiv = 0;

	/* If PLL is the clock source, PLL has already been set up. */
	if ((STM32_RCC_CFGR & STM32_RCC_CFGR_SWS_MASK) == 
	    STM32_RCC_CFGR_SWS_PLL)
		return;

	/* Ensure that HSE/HSI is ON */
	if (!(STM32_RCC_CR & clk_check_mask)) {
		/* Enable High Speed Oscillator */
		STM32_RCC_CR |= clk_enable_mask;
		/* Wait for HSE/HSI to be ready */
		while (!(STM32_RCC_CR & clk_check_mask))
			;
	}

#ifdef HSE_CLOCK
	/* Ensure that HSE is ON if we have one. */
	if (!(STM32_RCC_CR & STM32_RCC_CR_HSERDY)) {
		STM32_RCC_CR |= STM32_RCC_CR_HSEON;
		while (!(STM32_RCC_CR & STM32_RCC_CR_HSERDY));
	}
#endif


	/* PLL input must be between 1-2MHz, near 2 */
	/* Valid values 2-63 */
	plldiv = (srcclock + STM32F4_PLL_REQ - 1) / STM32F4_PLL_REQ;
	pllinputclock = srcclock / plldiv;

	/* PLL output clock: Must be 100-432MHz */
	/* Valid values 50-432, we'll get 336MHz */
	pllmult = (STM32F4_VCO_CLOCK + (pllinputclock / 2)) / pllinputclock;
	vcoclock = pllinputclock * pllmult;

	/* CPU/System clock: Below 180MHz */
	/* We'll do 84MHz */
	systemclock = vcoclock / 4;
	systemdivq = 1;
	/* USB clock = 48MHz exactly */
	usbdiv = (vcoclock + (STM32F4_USB_REQ / 2)) / STM32F4_USB_REQ;
	/* SYSTEM/I2S: same system clock */
	i2sdiv = (vcoclock + (systemclock / 2)) / systemclock;

	/* All IO clocks at 42MHz */
	/* AHB Prescalar */
	ahbpre = 0x8;   /* AHB = system clock / 2*/
	/* NOTE: If apbXpre is not 0, timers are x2 clocked. RM0390 Fig. 13 */
	apb1pre = 0;  /* APB1 = AHB */
	apb2pre = 0;  /* APB2 = AHB */

#ifdef HSE_CLOCK
	/* RTC clock = 1MHz */
	rtcdiv = (HSE_CLOCK + (STM32F4_RTC_REQ / 2)) / STM32F4_RTC_REQ;
#endif

	/* Set up PLL */
	STM32_RCC_PLLCFGR = 
		PLLCFGR_PLLM(plldiv) |
		PLLCFGR_PLLN(pllmult) |
		PLLCFGR_PLLP(systemdivq) |
#if defined(CONFIG_CLOCK_HSE)
		PLLCFGR_PLLSRC_HSE |
#else
		PLLCFGR_PLLSRC_HSI |
#endif
		PLLCFGR_PLLQ(usbdiv) | 
		PLLCFGR_PLLR(i2sdiv); 


	/* Enable the PLL */
	STM32_RCC_CR |= STM32_RCC_CR_PLLON;

	/* Wait until the PLL is ready */
	while (!(STM32_RCC_CR & STM32_RCC_CR_PLLRDY))
		;

	/* Switch SYSCLK to PLL, setup prescalars */
	/* NOTE: Sweetberry requires MCO2 <- HSE @ 24MHz */ 
	STM32_RCC_CFGR =
		(2 << 30) |  /* MCO2 <- HSE */
		(0 << 30) |  /* MCO2 <- SYSCLK */
		(0 << 27) |  /* MCO2 div / 4 */
		(6 << 24) |  /* MCO1 div / 4 */
		(3 << 21) |  /* MCO1 <- PLL */
		CFGR_RTCPRE(rtcdiv) |
		CFGR_PPRE2(apb2pre) |
		CFGR_PPRE1(apb1pre) |
		CFGR_HPRE(ahbpre) |
		STM32_RCC_CFGR_SW_PLL;
  
	/* Wait until the PLL is the clock source */
	if ((STM32_RCC_CFGR & STM32_RCC_CFGR_SWS_MASK) == 
	    STM32_RCC_CFGR_SWS_PLL)
		;

	/* Setup RTC Clock input */
	STM32_RCC_BDCR |= STM32_RCC_BDCR_BDRST;
#ifdef HSE_CLOCK
	STM32_RCC_BDCR = STM32_RCC_BDCR_RTCEN | BCDR_RTCSEL(BDCR_SRC_HSE);
#else
	/* Ensure that LSI is ON */
	if (!(STM32_RCC_CSR & STM32_RCC_CSR_LSIRDY)) {
		STM32_RCC_CSR |= STM32_RCC_CSR_LSION;
		while (!(STM32_RCC_CSR & STM32_RCC_CSR_LSIRDY));
	}
	STM32_RCC_BDCR = STM32_RCC_BDCR_RTCEN | BCDR_RTCSEL(BDCR_SRC_LSI);
#endif
}

int clock_get_freq(void)
{
	return STM32F4_IO_CLOCK;
}

void clock_wait_bus_cycles(enum bus_type bus, uint32_t cycles)
{
	volatile uint32_t dummy __attribute__((unused));

	if (bus == BUS_AHB) {
		while (cycles--)
			dummy = STM32_DMA_GET_ISR(0);
	} else { /* APB */
		while (cycles--)
			dummy = STM32_USART_BRR(STM32_USART1_BASE);
	}
}

void clock_enable_module(enum module_id module, int enable)
{
	if (module == MODULE_USB) {
		if (enable) {
			STM32_RCC_AHB2ENR |= STM32_RCC_AHB2ENR_OTGFSEN;
			STM32_RCC_AHB1ENR |= STM32_RCC_AHB1ENR_OTGHSEN | STM32_RCC_AHB1ENR_OTGHSULPIEN;
		} else {
			STM32_RCC_AHB2ENR &= ~STM32_RCC_AHB2ENR_OTGFSEN;
			STM32_RCC_AHB1ENR &= ~STM32_RCC_AHB1ENR_OTGHSEN &
				     ~STM32_RCC_AHB1ENR_OTGHSULPIEN;
		}
		return;
	} else if (module == MODULE_I2C) {
		if (enable) {
			/* Enable clocks to I2C modules if necessary */
			STM32_RCC_APB1ENR |=
				STM32_RCC_I2C1EN | STM32_RCC_I2C2EN |
				STM32_RCC_I2C3EN | STM32_RCC_FMPI2C4EN;
			STM32_RCC_DCKCFGR2 = (STM32_RCC_DCKCFGR2 & ~DCKCFGR2_FMPI2C1SEL_MASK) |
				DCKCFGR2_FMPI2C1SEL(FMPI2C1SEL_APB);
		} else {
			STM32_RCC_APB1ENR &=
				~(STM32_RCC_I2C1EN | STM32_RCC_I2C2EN |
				  STM32_RCC_I2C3EN | STM32_RCC_FMPI2C4EN);
		}
		return;
	}

	CPRINTS("Module %d is not supported for clock %s\n",
		module, enable ? "enable" : "disable");
}

void rtc_init(void)
{
	rtc_unlock_regs();

	/* Enter RTC initialize mode */
	STM32_RTC_ISR |= STM32_RTC_ISR_INIT;
	while (!(STM32_RTC_ISR & STM32_RTC_ISR_INITF))
		;

	/* Set clock prescalars: Needs two separate writes. */
	STM32_RTC_PRER = 
		(STM32_RTC_PRER & ~STM32_RTC_PRER_S_MASK) | RTC_PREDIV_S;
	STM32_RTC_PRER = 
		(STM32_RTC_PRER & ~STM32_RTC_PRER_A_MASK) | (RTC_PREDIV_A << 16);

	/* Start RTC timer */
	STM32_RTC_ISR &= ~STM32_RTC_ISR_INIT;
	while (STM32_RTC_ISR & STM32_RTC_ISR_INITF)
		;

	/* Enable RTC alarm interrupt */
	STM32_RTC_CR |= STM32_RTC_CR_ALRAIE | STM32_RTC_CR_BYPSHAD;
	STM32_EXTI_RTSR |= EXTI_RTC_ALR_EVENT;
	task_enable_irq(STM32_IRQ_RTC_ALARM);

	rtc_lock_regs();
}

void clock_init(void)
{
	/*
	 * The initial state :
	 *  SYSCLK from HSI (=8MHz), no divider on AHB, APB1, APB2
	 *  PLL unlocked, RTC enabled on LSE
	 */

	/*
	 * put 1 Wait-State for flash access to ensure proper reads at 48Mhz
	 * and enable prefetch buffer.
	 */
	/* Enable data and instruction cache. */
	/*STM32_FLASH_ACR = STM32_FLASH_ACR_LATENCY | STM32_FLASH_ACR_PRFTEN |
			STM32_FLASH_ACR_ICEN | STM32_FLASH_ACR_DCEN;*/
	STM32_FLASH_ACR = STM32_FLASH_ACR_LATENCY | STM32_FLASH_ACR_PRFTEN;

	config_hispeed_clock();

	rtc_init();
}

/*****************************************************************************/
/* Console commands */

#ifdef CONFIG_CMD_RTC_ALARM
static int command_rtc_alarm_test(int argc, char **argv)
{
	int s = 1, us = 0;
	uint32_t rtc, rtcss;
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

	set_rtc_alarm(s, us, &rtc, &rtcss);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rtc_alarm, command_rtc_alarm_test,
			"[seconds [microseconds]]",
			"Test alarm",
			NULL);
#endif /* CONFIG_CMD_RTC_ALARM */

#if defined(CONFIG_LOW_POWER_IDLE) && defined(CONFIG_COMMON_RUNTIME)
#ifdef CONFIG_CMD_IDLE_STATS
/**
 * Print low power idle statistics
 */
static int command_idle_stats(int argc, char **argv)
{
	timestamp_t ts = get_time();

	ccprintf("Num idle calls that sleep:           %d\n", idle_sleep_cnt);
	ccprintf("Num idle calls that deep-sleep:      %d\n", idle_dsleep_cnt);
	ccprintf("Time spent in deep-sleep:            %.6lds\n",
			idle_dsleep_time_us);
	ccprintf("Total time on:                       %.6lds\n", ts.val);
	ccprintf("Deep-sleep closest to wake deadline: %dus\n",
			dsleep_recovery_margin_us);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(idlestats, command_idle_stats,
			"",
			"Print last idle stats",
			NULL);
#endif /* CONFIG_CMD_IDLE_STATS */
#endif

