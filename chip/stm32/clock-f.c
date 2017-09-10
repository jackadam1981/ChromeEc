/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks and power management settings */

#include "chipset.h"
#include "clock.h"
#include "clock-f.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "hooks.h"
#include "host_command.h"
#include "hwtimer.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CLOCK, outstr)
#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ## args)

#define SECS_PER_DAY        (86400)
#define SECS_PER_YEAR       (365 * SECS_PER_DAY)
#define SECS_TILL_YEAR_2K   (946684800)
#define IS_LEAP_YEAR(x)     \
	((x % 4 == 0) && ((x % 100 != 0) || (x % 400 == 0)))

#if defined(CONFIG_CMD_RTC) || defined(CONFIG_HOSTCMD_RTC)
static uint32_t days_since_year_start[12] = {
0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
#endif

/* Convert between RTC regs in BCD and seconds */

static uint32_t rtc_tr_to_sec(uint32_t rtc_tr)
{
	uint32_t sec;

	/* convert the hours field */
	sec = (((rtc_tr & 0x300000) >> 20) * 10 +
	       ((rtc_tr & 0xf0000) >> 16)) * 3600;
	/* convert the minutes field */
	sec += (((rtc_tr & 0x7000) >> 12) * 10 + ((rtc_tr & 0xf00) >> 8)) * 60;
	/* convert the seconds field */
	sec += ((rtc_tr & 0x70) >> 4) * 10 + (rtc_tr & 0xf);
	return sec;
}

uint32_t rtc_to_sec(struct rtc_time_reg rtc)
{
	uint32_t sec = 0;
#if defined(CONFIG_CMD_RTC) || defined(CONFIG_HOSTCMD_RTC)
	int year;
	int month;
	int day;
	int i;

	/* convert the years field */
	year = (((rtc.rtc_dr & 0xf00000) >> 20) * 10 +
	      ((rtc.rtc_dr & 0xf0000) >> 16));
	sec = year * SECS_PER_YEAR;
	for (i = 0; i < year; i++) {
		if (IS_LEAP_YEAR(i))
			sec += SECS_PER_DAY;
	}

	/* convert the months and days field */
	month = (((rtc.rtc_dr & 0x1000) >> 12) * 10 +
		((rtc.rtc_dr & 0xf00) >> 8));
	day = ((rtc.rtc_dr & 0x30) >> 4) * 10 + (rtc.rtc_dr & 0xf);
	day += days_since_year_start[month - 1];
	day += (IS_LEAP_YEAR(year) && month > 2);

	sec += (day - 1) * SECS_PER_DAY;

	/* add the accumulated time in seconds from 1970 to 2000 */
	sec += SECS_TILL_YEAR_2K;
#endif
	return sec + (rtc_ssr_to_us(rtc.rtc_ssr) / SECOND) +
	       rtc_tr_to_sec(rtc.rtc_tr);
}

static uint32_t sec_to_rtc_tr(uint32_t sec)
{
	uint32_t rtc_tr;

	/* convert the hours field */
	rtc_tr = ((sec / 36000) << 20) | (((sec / 3600) % 10) << 16);
	/* convert the minutes field */
	rtc_tr |= (((sec % 3600) / 600) << 12) | (((sec % 600) / 60) << 8);
	/* convert the seconds field */
	rtc_tr |= (((sec % 60) / 10) << 4) | (sec % 10);

	return rtc_tr;
}

struct rtc_time_reg sec_to_rtc(uint32_t sec)
{
	struct rtc_time_reg rtc;
#if defined(CONFIG_CMD_RTC) || defined(CONFIG_HOSTCMD_RTC)
	int year;
	int month;
	int day;
	uint8_t is_leap_year;
	int i;

	/* rtc time must be after year 2000 */
	sec = (sec > SECS_TILL_YEAR_2K) ? (sec - SECS_TILL_YEAR_2K) : 0;

	/* convert the year, months and days */
	day = sec / SECS_PER_DAY;

	year = day / 365;
	is_leap_year = IS_LEAP_YEAR(year);
	day %= 365;
	for (i = 0; i < year; i++) {
		if (IS_LEAP_YEAR(i))
			day -= 1;
	}
	if (++day <= 0) {
		year -= 1;
		is_leap_year = IS_LEAP_YEAR(year);
		day += is_leap_year ? 366 : 365;
	}
	month = 1;
	do {
		if (days_since_year_start[month] +
		    (is_leap_year && (month > 2)) >= day)
			break;
		month++;
	} while (month < 12);
	day -= days_since_year_start[month - 1] +
	       (is_leap_year && (month > 2));

	rtc.rtc_dr = ((year / 10) << 20) | ((year % 10) << 16);
	rtc.rtc_dr |= ((month / 10) << 12) | ((month % 10) << 8);
	rtc.rtc_dr |= ((day / 10) << 4) | (day % 10);
#endif
	rtc.rtc_tr = sec_to_rtc_tr(sec % SECS_PER_DAY);
	rtc.rtc_ssr = 0;

	return rtc;
}

/* Return sub-10-sec time diff between two rtc readings */
int32_t get_rtc_diff(struct rtc_time_reg rtc0, struct rtc_time_reg rtc1)
{
	int32_t diff;

	/* Note: This only looks at the diff mod 10 seconds */
	diff =  ((rtc1.rtc_tr & 0xf) * SECOND +
		 rtc_ssr_to_us(rtc1.rtc_ssr)) -
		((rtc0.rtc_tr & 0xf) * SECOND +
		 rtc_ssr_to_us(rtc0.rtc_ssr));

	return (diff < 0) ? (diff + 10 * SECOND) : diff;
}

void rtc_read(struct rtc_time_reg *rtc)
{
	/*
	 * Read current time synchronously. Each registers must be read
	 * twice with identical values because glitches may occur for reads
	 * close to the RTCCLK edge.
	 */
	do {
		rtc->rtc_ssr = STM32_RTC_SSR;
	} while (rtc->rtc_ssr != STM32_RTC_SSR);

	do {
		rtc->rtc_tr = STM32_RTC_TR;
	} while (rtc->rtc_tr != STM32_RTC_TR);

	do {
		rtc->rtc_dr = STM32_RTC_DR;
	} while (rtc->rtc_dr != STM32_RTC_DR);
}

void set_rtc_alarm(uint32_t delay_s, uint32_t delay_us,
		   struct rtc_time_reg *rtc)
{
	uint32_t alarm_sec, alarm_us;

	/* Alarm must be within 1 day (86400 seconds) */

	ASSERT((delay_s + delay_us / SECOND) < SECS_PER_DAY);

	rtc_unlock_regs();

	/* Make sure alarm is disabled */
	STM32_RTC_CR &= ~STM32_RTC_CR_ALRAE;
	while (!(STM32_RTC_ISR & STM32_RTC_ISR_ALRAWF))
		;
	STM32_RTC_ISR &= ~STM32_RTC_ISR_ALRAF;

	rtc_read(rtc);

	/* Calculate alarm time */
	alarm_sec = rtc_tr_to_sec(rtc->rtc_tr) + delay_s;
	alarm_us = rtc_ssr_to_us(rtc->rtc_ssr) + delay_us;
	alarm_sec = alarm_sec + alarm_us / SECOND;
	alarm_us = alarm_us % SECOND;
	/*
	 * If seconds is greater than 1 day, subtract by 1 day to deal with
	 * 24-hour rollover.
	 */
	if (alarm_sec >= 86400)
		alarm_sec -= 86400;

	/* Set alarm time */
	STM32_RTC_ALRMAR = sec_to_rtc_tr(alarm_sec);
	STM32_RTC_ALRMASSR = us_to_rtc_ssr(alarm_us);
	/* Check for match on hours, minutes, seconds, and subsecond */
	STM32_RTC_ALRMAR |= 0xc0000000;
	STM32_RTC_ALRMASSR |= 0x0f000000;

	/* Enable alarm and alarm interrupt */
	STM32_EXTI_PR = EXTI_RTC_ALR_EVENT;
	STM32_EXTI_IMR |= EXTI_RTC_ALR_EVENT;
	STM32_RTC_CR |= STM32_RTC_CR_ALRAE;

	rtc_lock_regs();
}

uint32_t get_rtc_alarm(void)
{
	struct rtc_time_reg now;
	uint32_t now_sec;
	uint32_t alarm_sec;

	if (!(STM32_RTC_CR & STM32_RTC_CR_ALRAE))
		return 0;

	rtc_read(&now);

	now_sec = rtc_tr_to_sec(now.rtc_tr) +
		  rtc_ssr_to_us(now.rtc_ssr) / SECOND;
	alarm_sec = rtc_tr_to_sec(STM32_RTC_ALRMAR) +
		    rtc_ssr_to_us(STM32_RTC_ALRMASSR) / SECOND;

	return ((alarm_sec < now_sec) ? SECS_PER_DAY : 0) +
	       (alarm_sec - now_sec);
}

void reset_rtc_alarm(struct rtc_time_reg *rtc)
{
	rtc_unlock_regs();

	/* Disable alarm */
	STM32_RTC_CR &= ~STM32_RTC_CR_ALRAE;
	STM32_RTC_ISR &= ~STM32_RTC_ISR_ALRAF;

	/* Disable RTC alarm interrupt */
	STM32_EXTI_IMR &= ~EXTI_RTC_ALR_EVENT;
	STM32_EXTI_PR = EXTI_RTC_ALR_EVENT;

	/* Read current time */
	rtc_read(rtc);

	rtc_lock_regs();
}

void __rtc_alarm_irq(void)
{
	struct rtc_time_reg rtc;

	reset_rtc_alarm(&rtc);
}
DECLARE_IRQ(STM32_IRQ_RTC_ALARM, __rtc_alarm_irq, 1);

__attribute__((weak))
int clock_get_timer_freq(void)
{
	return clock_get_freq();
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
	STM32_FLASH_ACR = STM32_FLASH_ACR_LATENCY | STM32_FLASH_ACR_PRFTEN;

#ifdef CHIP_FAMILY_STM32F4
	/* Enable data and instruction cache. */
	STM32_FLASH_ACR |= STM32_FLASH_ACR_DCEN | STM32_FLASH_ACR_ICEN;
#endif

	config_hispeed_clock();

	rtc_init();
}

#ifdef CHIP_FAMILY_STM32F4
void reset_flash_cache(void)
{
	/* Disable data and instruction cache. */
	STM32_FLASH_ACR &= ~(STM32_FLASH_ACR_DCEN | STM32_FLASH_ACR_ICEN);

	/* Reset data and instruction cache */
	STM32_FLASH_ACR |= STM32_FLASH_ACR_DCRST | STM32_FLASH_ACR_ICRST;
}
DECLARE_HOOK(HOOK_SYSJUMP, reset_flash_cache, HOOK_PRIO_DEFAULT);
#endif

/*****************************************************************************/
/* Console commands */

#ifdef CONFIG_CMD_RTC
void print_system_rtc(enum console_channel ch)
{
	uint32_t sec;
	struct rtc_time_reg rtc;

	rtc_read(&rtc);
	sec = rtc_to_sec(rtc);
	cprintf(ch, "RTC: 0x%08x (%d.00 s)\n", sec, sec);
}

static int command_system_rtc(int argc, char **argv)
{
	char *e;
	uint32_t t;

	if (argc == 3 && !strcasecmp(argv[1], "set")) {
		t = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;
		rtc_set(t);
	} else if (argc > 1)
		return EC_ERROR_INVAL;

	print_system_rtc(CC_COMMAND);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rtc, command_system_rtc,
		"[set <seconds>]",
		"Get/set real-time clock");

#ifdef CONFIG_CMD_RTC_ALARM
static int command_rtc_alarm_test(int argc, char **argv)
{
	int s = 1, us = 0;
	struct rtc_time_reg rtc;
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

	set_rtc_alarm(s, us, &rtc);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rtc_alarm, command_rtc_alarm_test,
			"[seconds [microseconds]]",
			"Test alarm");
#endif /* CONFIG_CMD_RTC_ALARM */
#endif /* CONFIG_CMD_RTC */

/*****************************************************************************/
/* Host commands */

#ifdef CONFIG_HOSTCMD_RTC
static int system_rtc_get_value(struct host_cmd_handler_args *args)
{
	struct ec_response_rtc *r = args->response;
	struct rtc_time_reg rtc;

	rtc_read(&rtc);
	r->time = rtc_to_sec(rtc);
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_RTC_GET_VALUE,
		system_rtc_get_value,
		EC_VER_MASK(0));

static int system_rtc_set_value(struct host_cmd_handler_args *args)
{
	const struct ec_params_rtc *p = args->params;

	rtc_set(p->time);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_RTC_SET_VALUE,
		system_rtc_set_value,
		EC_VER_MASK(0));

static int system_rtc_set_alarm(struct host_cmd_handler_args *args)
{
	struct rtc_time_reg rtc;
	const struct ec_params_rtc *p = args->params;

	set_rtc_alarm(p->time, 0, &rtc);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_RTC_SET_ALARM,
		system_rtc_set_alarm,
		EC_VER_MASK(0));

static int system_rtc_get_alarm(struct host_cmd_handler_args *args)
{
	struct ec_response_rtc *r = args->response;

	r->time = get_rtc_alarm();
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_RTC_GET_ALARM,
		system_rtc_get_alarm,
		EC_VER_MASK(0));

#endif /* CONFIG_HOSTCMD_RTC */
