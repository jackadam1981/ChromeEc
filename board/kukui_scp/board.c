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
#include "memmap.h"
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

static int command_memmaptest(int argc, char **argv)
{
	uintptr_t scp = 0x0f001000;
	uintptr_t ap, scp2;
	int i;

	for (i = 0; i < 15; i++, scp += 0x10001000) {
		int ret;

		ret = memmap_scp_to_ap(scp, &ap);
		if (ret != EC_SUCCESS) {
			ccprintf("%08x INVAL\n", scp);
			continue;
		}

		ret = memmap_ap_to_scp(ap, &scp2);

		ccprintf("%08x %08x %s", scp, ap,
			(ret == EC_SUCCESS && scp == scp2) ? "OK" : "BAD");
	}

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(memmaptest, command_memmaptest,
			     NULL,
			     "Do remmap test");
