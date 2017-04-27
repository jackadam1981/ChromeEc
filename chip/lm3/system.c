/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* System module for Chrome EC : LM3 hardware specific implementation */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "host_command.h"
#include "panic.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Indices for hibernate data registers */
enum hibdata_index {
	HIBDATA_INDEX_SCRATCHPAD,           /* General-purpose scratchpad */
	HIBDATA_INDEX_WAKE,                 /* Wake reasons for hibernate */
	HIBDATA_INDEX_SAVED_RESET_FLAGS,    /* Saved reset flags */
#ifdef CONFIG_SOFTWARE_PANIC
	HIBDATA_INDEX_SAVED_PANIC_REASON,   /* Saved panic reason */
	HIBDATA_INDEX_SAVED_PANIC_INFO,     /* Saved panic data */
	HIBDATA_INDEX_SAVED_PANIC_EXCEPTION /* Saved panic exception code */
#endif
};

/* Flags for HIBDATA_INDEX_WAKE */
#define HIBDATA_WAKE_RTC        (1 << 0)  /* RTC alarm */
#define HIBDATA_WAKE_HARD_RESET (1 << 1)  /* Hard reset via short RTC alarm */
#define HIBDATA_WAKE_PIN        (1 << 2)  /* Wake pin */

/*
 * Time to hibernate to trigger a power-on reset.  50 ms is sufficient for the
 * EC itself, but we need a longer delay to ensure the rest of the components
 * on the same power rail are reset and 5VALW has dropped.
 */
#define HIB_RESET_USEC 1000000

/*
 * Convert between microseconds and the hibernation module RTC subsecond
 * register which has 15-bit resolution. Divide down both numerator and
 * denominator to avoid integer overflow while keeping the math accurate.
 */
#define HIB_RTC_USEC_TO_SUBSEC(us) ((us) * (32768/64) / (1000000/64))
#define HIB_RTC_SUBSEC_TO_USEC(ss) ((ss) * (1000000/64) / (32768/64))

/*
 * A3 and earlier chip stepping has a problem accessing flash during shutdown.
 * To work around that, we jump to RAM before hibernating.  This function must
 * live in RAM.  It must be called with interrupts disabled, cannot call other
 * functions, and can't be declared static (or else the compiler optimizes it
 * into the main hibernate function.
 */
void  __attribute__((noinline)) __attribute__((section(".iram.text")))
__enter_hibernate(int hibctl)
{
}

/**
 * Read the real-time clock.
 *
 * @param ss_ptr       Destination for sub-seconds value, if not null.
 *
 * @return the real-time clock seconds value.
 */
uint32_t system_get_rtc_sec_subsec(uint32_t *ss_ptr)
{
	return 0;
}

timestamp_t system_get_rtc(void)
{
	timestamp_t time;

	return time;
}

/**
 * Set the real-time clock.
 *
 * @param seconds	New clock value.
 */
void system_set_rtc(uint32_t seconds)
{
}

/**
 * Use hibernate module to set up an RTC interrupt at a given
 * time from now
 *
 * @param seconds      Number of seconds before RTC interrupt
 * @param microseconds Number of microseconds before RTC interrupt
 */
void system_set_rtc_alarm(uint32_t seconds, uint32_t microseconds)
{
}

/**
 * Disable and clear the RTC interrupt.
 */
void system_reset_rtc_alarm(void)
{
}

void system_pre_init(void)
{
}

void system_reset(int flags)
{
	uint32_t save_flags = 0;

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable();

	/* Save current reset reasons if necessary */
	if (flags & SYSTEM_RESET_PRESERVE_FLAGS)
		save_flags = system_get_reset_flags() | RESET_FLAG_PRESERVED;

	if (flags & SYSTEM_RESET_LEAVE_AP_OFF)
		save_flags |= RESET_FLAG_AP_OFF;
		CPU_NVIC_APINT = 0x05fa0004;

	/* Spin and wait for reboot; should never return */
	while (1)
		;
}

int system_set_scratchpad(uint32_t value)
{
	return 0;
}

uint32_t system_get_scratchpad(void)
{
	return 0;
}

const char *system_get_chip_vendor(void)
{
	return "ti";
}

static char to_hex(int x)
{
	if (x >= 0 && x <= 9)
		return '0' + x;
	return 'a' + x - 10;
}

const char *system_get_chip_id_string(void)
{
	static char str[15] = "Unknown-";
	char *p = str + 8;
	uint32_t did = LM3_SYSTEM_DID1 >> 16;

	if (*p)
		return (const char *)str;

	*p = to_hex(did >> 12);
	*(p + 1) = to_hex((did >> 8) & 0xf);
	*(p + 2) = to_hex((did >> 4) & 0xf);
	*(p + 3) = to_hex(did & 0xf);
	*(p + 4) = '\0';

	return (const char *)str;
}

const char *system_get_raw_chip_name(void)
{
	switch ((LM3_SYSTEM_DID1 & 0xffff0000) >> 16) {
	case 0x10de:
		return "tm4e1g31h6zrb";
	case 0x10e2:
		return "lm4fsxhh5bb";
	case 0x10e3:
		return "lm4fs232h5bb";
	case 0x10e4:
		return "lm4fs99h5bb";
	case 0x10e6:
		return "lm4fs1ah5bb";
	case 0x10ea:
		return "lm4fs1gh5bb";
	default:
		return system_get_chip_id_string();
	}
}

const char *system_get_chip_name(void)
{
	const char *raw_chip_name = system_get_raw_chip_name();

	return raw_chip_name;
}

int system_get_bbram(enum system_bbram_idx idx, uint8_t *value)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int system_set_bbram(enum system_bbram_idx idx, uint8_t value)
{
	return EC_ERROR_UNIMPLEMENTED;
}

const char *system_get_chip_revision(void)
{
	static char rev[3];

	/* Extract the major[15:8] and minor[7:0] revisions. */
	rev[0] = 'A' + ((LM3_SYSTEM_DID0 >> 8) & 0xff);
	rev[1] = '0' + (LM3_SYSTEM_DID0 & 0xff);
	rev[2] = 0;

	return rev;
}

/*****************************************************************************/
/* Console commands */
#ifdef CONFIG_CMD_RTC
void print_system_rtc(enum console_channel ch)
{
	uint32_t rtc;
	uint32_t rtcss;

	rtc = system_get_rtc_sec_subsec(&rtcss);
	cprintf(ch, "RTC: 0x%08x.%04x (%d.%06d s)\n",
		 rtc, rtcss, rtc, HIB_RTC_SUBSEC_TO_USEC(rtcss));
}

static int command_system_rtc(int argc, char **argv)
{
	if (argc == 3 && !strcasecmp(argv[1], "set")) {
		char *e;
		uint32_t t = strtoi(argv[2], &e, 0);

		if (*e)
			return EC_ERROR_PARAM2;

		system_set_rtc(t);
	} else if (argc > 1) {
		return EC_ERROR_INVAL;
	}

	print_system_rtc(CC_COMMAND);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rtc, command_system_rtc,
			"[set <seconds>]",
			"Get/set real-time clock");

#ifdef CONFIG_CMD_RTC_ALARM
/**
 * Test the RTC alarm by setting an interrupt on RTC match.
 */
static int command_rtc_alarm_test(int argc, char **argv)
{
	int s = 1, us = 0;
	char *e;

	ccprintf("Setting RTC alarm\n");
	system_enable_hib_interrupt();

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

	system_set_rtc_alarm(s, us);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rtc_alarm, command_rtc_alarm_test,
			"[seconds [microseconds]]",
			"Test alarm");
#endif /* CONFIG_CMD_RTC_ALARM */
#endif /* CONFIG_CMD_RTC */
