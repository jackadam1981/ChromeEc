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

void clock_init(void)
{
/* Initialize the UART and put it on CLK_M so we can use it during clock_init().
 * Will later move it to PLLP in clock_config(). The divisor must be very small
 * to accomodate 12KHz OSCs, so we override the 16.0 UART divider with the 15.1
 * CLK_SOURCE divider to get more precision. (This might still not be enough for
 * some OSCs... if you use 13KHz, be prepared to have a bad time.) The 1800 has
 * been determined through trial and error (must lead to div 13 at 24MHz). */
	AVP_CAR_CLK_SRC_UARTA = (CLK_M << CLK_SOURCE_SHIFT | CLK_UART_DIV_OVERRIDE | CLK_DIVIDER(TEGRA_CLK_M_KHZ, 1800));
}

int clock_get_freq(void)
{
	return freq;
}

/**
 * Enable clock to specified peripheral
 *
 * @param offset Should be element of clock_gate_offsets enum.
 *               Bits 8-15 specify the ECPM offset of the specific clock reg.
 *               Bits 0-7 specify the mask for the clock register.
 * @param mask   Unused
 * @param mode   Unused
 */
void clock_enable_peripheral(uint32_t offset, uint32_t mask, uint32_t mode)
{
	AVP_CAR_OUT_ENB_L |= offset;
	/* delay 2us ? */
	AVP_CAR_RST_DEV_L &= ~offset;
}

/**
 * Disable clock to specified peripheral
 *
 * @param offset Should be element of clock_gate_offsets enum.
 *               Bits 8-15 specify the ECPM offset of the specific clock reg.
 *               Bits 0-7 specify the mask for the clock register.
 * @param mask   Unused
 * @param mode   Unused
 */
void clock_disable_peripheral(uint32_t offset, uint32_t mask, uint32_t mode)
{
	AVP_CAR_OUT_ENB_L &= ~offset;
}
