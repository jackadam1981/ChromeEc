/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Kukui SCP configuration */

#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Build GPIO tables */
void eint_event(enum gpio_signal signal);

#include "gpio_list.h"


void eint_event(enum gpio_signal signal)
{
	ccprintf("EINT event: %d\n", signal);
}

/* Initialize board.  */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_EINT5_TP);
	gpio_enable_interrupt(GPIO_EINT6_TP);
	gpio_enable_interrupt(GPIO_EINT7_TP);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

#include "watchdog.h"

static void busy(void) {
	int i, j;
	const int it = 20000;
	const int len = 0x100;
	char buffer[len];
	timestamp_t start;
	uint64_t val;

	start = get_time();
	for (j = 0; j < len; j++)
		buffer[j] = j;

	val = 0;
	for (i = 0; i < it; i++) {
		for (j = 0; j < len; j++)
			val += buffer[j];
	}
	ccprintf("busyloop: %d us (val: %lx)\n", time_since32(start), val);
	cflush();
}

static int command_busytest(int argc, char **argv)
{
	int k;

	for (k = 0; k < 20; k++) {
		busy();
		watchdog_reload();
	}

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(busytest, command_busytest,
			     NULL,
			     "Busy loop");

static void busy_loop(void);
DECLARE_DEFERRED(busy_loop);
static void busy_loop(void) {
	busy();
	hook_call_deferred(&busy_loop_data, 1*MSEC);
}
DECLARE_HOOK(HOOK_INIT, busy_loop, HOOK_PRIO_LAST);
