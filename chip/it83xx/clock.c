/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks and power management settings */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros. */
#define CPUTS(outstr) cputs(CC_CLOCK, outstr)
#define CPRINTF(format, args...) cprintf(CC_CLOCK, format, ## args)

static int freq;

struct clock_gate_ctrl {
	volatile uint8_t *reg;
	uint8_t mask;
};

/*
 * List of gate control registers and their masks for the corresponding
 * indices listed in enum clock_gate_offsets.
 */
static const struct clock_gate_ctrl gate_ctrl[] = {
	{IT83XX_ECPM_CGCTRL2R, 0x40},
	{IT83XX_ECPM_CGCTRL2R, 0x20},
	{IT83XX_ECPM_CGCTRL2R, 0x10},
	{IT83XX_ECPM_CGCTRL3R, 0x20},
	{IT83XX_ECPM_CGCTRL3R, 0x08},
	{IT83XX_ECPM_CGCTRL3R, 0x04},
	{IT83XX_ECPM_CGCTRL3R, 0x02},
	{IT83XX_ECPM_CGCTRL3R, 0x01},
	{IT83XX_ECPM_CGCTRL4R, 0x02},
	{IT83XX_ECPM_CGCTRL4R, 0x01},
};
BUILD_ASSERT(ARRAY_SIZE(gate_ctrl) == CGC_COUNT);

void clock_init(void)
{
#if PLL_CLOCK == 48000000
	/* Set PLL frequency to 48MHz. */
	IT83XX_ECPM_PLLFREQR = 0x04;
	freq = PLL_CLOCK;
#else
#error "Support only for PLL clock speed of 48MHz."
#endif

	/* Set EC Clock Frequency to PLL frequency. */
	IT83XX_ECPM_SCDCR3 &= 0xf0;

	/* Turn off auto clock gating. */
	IT83XX_ECPM_AUTOCG = 0x00;
}

int clock_get_freq(void)
{
	return freq;
}

void clock_enable_peripheral(uint32_t offset, uint32_t mask, uint32_t mode)
{
	*(gate_ctrl[offset].reg) &= ~gate_ctrl[offset].mask;
}

void clock_disable_peripheral(uint32_t offset, uint32_t mask, uint32_t mode)
{
	uint8_t tmp_mask = 0;

	/* CGCTRL3R, bit 6, must always write a 1. */
	tmp_mask |= (gate_ctrl[offset].reg == IT83XX_ECPM_CGCTRL3R) ?
			0x40 : 0x00;

	*(gate_ctrl[offset].reg) |= gate_ctrl[offset].mask | tmp_mask;
}
