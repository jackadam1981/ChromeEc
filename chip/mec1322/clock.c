/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks and power management settings */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "module_id.h"
#include "registers.h"
#include "util.h"

#include "timer.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CLOCK, outstr)
#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ## args)

static int freq = 12000000;

enum cpu_freq {
	CPU_1MHZ = 1,
	CPU_3MHZ = 3,
	CPU_12MHZ = 12,
	CPU_48MHZ = 48,
};

void clock_wait_cycles(uint32_t cycles)
{
	asm("1: subs %0, #1\n"
	    "   bne 1b\n" :: "r"(cycles));
}

static int clock_set_freq(enum cpu_freq new_freq)
{
	MEC1322_PCR_PROC_CLK_CTL = 48 / new_freq;
	freq = new_freq * 1000000;
	return EC_SUCCESS;
}

int clock_get_freq(void)
{
	return freq;
}

void clock_enable_module(enum module_id module, int enable)
{
	static uint32_t clock_mask;
	int new_mask;

	if (enable)
		new_mask = clock_mask | (1 << module);
	else
		new_mask = clock_mask & ~(1 << module);

	/* Only change clock if needed */
	if ((!!new_mask) != (!!clock_mask))
		clock_set_freq(new_mask ? CPU_48MHZ : CPU_3MHZ);
	clock_mask = new_mask;
}

void clock_init(void)
{
	/* XOSEL = Single ended clock source */
	MEC1322_VBAT_CE |= 0x1;

	/* 32K clock enable */
	MEC1322_VBAT_CE |= 0x2;

	clock_set_freq(CPU_3MHZ);
}
