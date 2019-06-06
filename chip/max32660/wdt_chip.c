/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 Watchdog Module */

#include "clock.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "task.h"
#include "util.h"
#include "watchdog.h"
#include "console.h"
#include "registers.h"
#include "board.h"
#include "wdt_regs.h"

#define CPUTS(outstr) cputs(CC_COMMAND, outstr)
#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ##args)

/*
	MAX32660 valid mS times for watchdog at 96 MHz
		WDT_PERIOD_2_16  = 1 mS
		WDT_PERIOD_2_17  = 2 mS
		WDT_PERIOD_2_18  = 4 mS
		WDT_PERIOD_2_19  = 9 mS
		WDT_PERIOD_2_20  = 18 mS
		WDT_PERIOD_2_21  = 35 mS
		WDT_PERIOD_2_22  = 70 mS
		WDT_PERIOD_2_23  = 140 mS
		WDT_PERIOD_2_24  = 280 mS
		WDT_PERIOD_2_25  = 560 mS
		WDT_PERIOD_2_26  = 1120 mS
		WDT_PERIOD_2_27  = 2240 mS
		WDT_PERIOD_2_28  = 4470 mS
		WDT_PERIOD_2_29  = 8950 mS
		WDT_PERIOD_2_30  = 17900 mS
		WDT_PERIOD_2_31  = 35800 mS
*/

typedef enum {
	WDT_PERIOD_2_31 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW31, /**< Period 2^31 */
	WDT_PERIOD_2_30 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW30, /**< Period 2^30 */
	WDT_PERIOD_2_29 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW29, /**< Period 2^29 */
	WDT_PERIOD_2_28 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW28, /**< Period 2^28 */
	WDT_PERIOD_2_27 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW27, /**< Period 2^27 */
	WDT_PERIOD_2_26 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW26, /**< Period 2^26 */
	WDT_PERIOD_2_25 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW25, /**< Period 2^25 */
	WDT_PERIOD_2_24 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW24, /**< Period 2^24 */
	WDT_PERIOD_2_23 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW23, /**< Period 2^23 */
	WDT_PERIOD_2_22 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW22, /**< Period 2^22 */
	WDT_PERIOD_2_21 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW21, /**< Period 2^21 */
	WDT_PERIOD_2_20 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW20, /**< Period 2^20 */
	WDT_PERIOD_2_19 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW19, /**< Period 2^19 */
	WDT_PERIOD_2_18 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW18, /**< Period 2^18 */
	WDT_PERIOD_2_17 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW17, /**< Period 2^17 */
	WDT_PERIOD_2_16 =
		MXC_S_WDT_CTRL_INT_PERIOD_WDT2POW16, /**< Period 2^16 */
} wdt_period_t;

#define WATCHDOG_TIMER_PERIOD WDT_PERIOD_2_29

volatile int starve_dog = 0;

void watchdog_reload(void)
{
	if (!starve_dog) {
		/* Reset the watchdog */
		MXC_WDT0->rst = 0x00A5;
		MXC_WDT0->rst = 0x005A;
	}
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);

int watchdog_init(void)
{
	// WDT_SetResetPeriod(MXC_WDT0, WATCHDOG_TIMER_PERIOD);
	/* Set the Watchdog period */
	MXC_SETFIELD(MXC_WDT0->ctrl, MXC_F_WDT_CTRL_RST_PERIOD,
		     (WATCHDOG_TIMER_PERIOD << 4));

	/* We want the WD to reset us if it is not fed in time. */
	MXC_WDT0->ctrl |= MXC_F_WDT_CTRL_RST_EN;
	/* Enable the watchdog */
	MXC_WDT0->ctrl |= MXC_F_WDT_CTRL_WDT_EN;
	/* Reset the watchdog */
	MXC_WDT0->rst = 0x00A5;
	MXC_WDT0->rst = 0x005A;
	return EC_SUCCESS;
}

static int command_watchdog_test(int argc, char **argv)
{
	starve_dog = 1;

	CPRINTS("done command_watchdog_test.");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(wdttest, command_watchdog_test, "wdttest",
			"Force a WDT reset.");
